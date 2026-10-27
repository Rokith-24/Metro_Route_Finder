#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audit_worker.h"
#include "common.h"

#define PRODUCER_COUNT 8
#define ACTION_LIMIT 100000

typedef struct {
    volatile LONG accepted;
    volatile LONG rejected;
    volatile LONG unexpected;
} SharedStats;

static SharedStats stats;

static DWORD WINAPI racingProducer(LPVOID parameter)
{
    int id = (int)(INT_PTR)parameter;
    unsigned long sequence;
    for (sequence = 0; sequence < ACTION_LIMIT; ++sequence) {
        char action[64];
        char details[96];
        AppStatus status;
        (void)snprintf(action, sizeof(action), "SHUTDOWN_RACE_P%d_%lu", id, sequence);
        (void)snprintf(details, sizeof(details), "producer=%d sequence=%lu", id, sequence);
        status = queueAuditEvent("shutdown_race_test", action, details, APP_SUCCESS);
        if (status == APP_SUCCESS) {
            InterlockedIncrement(&stats.accepted);
        } else if (status == APP_ERROR) {
            InterlockedIncrement(&stats.rejected);
            return 0;
        } else {
            InterlockedIncrement(&stats.unexpected);
            return 1;
        }
    }
    return 0;
}

int main(void)
{
    HANDLE threads[PRODUCER_COUNT];
    int i;
    int failed = 0;

    memset(&stats, 0, sizeof(stats));
    for (i = 0; i < PRODUCER_COUNT; ++i) threads[i] = NULL;
    if (startAuditWorker() != APP_SUCCESS) {
        fprintf(stderr, "FAIL: could not start audit worker.\n");
        return EXIT_FAILURE;
    }

    for (i = 0; i < PRODUCER_COUNT; ++i) {
        threads[i] = CreateThread(NULL, 0, racingProducer,
                                  (LPVOID)(INT_PTR)i, 0, NULL);
        if (threads[i] == NULL) {
            fprintf(stderr, "FAIL: could not create producer thread %d.\n", i);
            failed = 1;
            break;
        }
    }

    /* Intentionally race shutdown against producers still submitting events. */
    Sleep(20);
    stopAuditWorker();

    for (i = 0; i < PRODUCER_COUNT; ++i) {
        if (threads[i] != NULL) {
            if (WaitForSingleObject(threads[i], INFINITE) != WAIT_OBJECT_0) {
                fprintf(stderr, "FAIL: producer thread %d did not finish.\n", i);
                failed = 1;
            }
            CloseHandle(threads[i]);
        }
    }

    if (InterlockedCompareExchange(&stats.unexpected, 0, 0) != 0) {
        fprintf(stderr, "FAIL: producer returned an unexpected status.\n");
        failed = 1;
    }
    if (InterlockedCompareExchange(&stats.accepted, 0, 0) <= 0) {
        fprintf(stderr, "FAIL: no events were accepted before shutdown.\n");
        failed = 1;
    }
    if (InterlockedCompareExchange(&stats.rejected, 0, 0) <= 0) {
        fprintf(stderr, "FAIL: producers did not observe shutdown.\n");
        failed = 1;
    }

    /* The API should reject calls cleanly after shutdown and remain restartable. */
    if (queueAuditEvent("test", "AFTER_STOP", "must reject", APP_SUCCESS) != APP_ERROR) {
        fprintf(stderr, "FAIL: queueing after stop should return APP_ERROR.\n");
        failed = 1;
    }
    if (startAuditWorker() != APP_SUCCESS) {
        fprintf(stderr, "FAIL: worker did not restart after shutdown.\n");
        failed = 1;
    } else {
        if (queueAuditEvent("test", "AFTER_RESTART", "restart works", APP_SUCCESS) != APP_SUCCESS) {
            fprintf(stderr, "FAIL: queueing after restart failed.\n");
            failed = 1;
        }
        stopAuditWorker();
    }

    if (failed) return EXIT_FAILURE;
    printf("Shutdown-race test passed: accepted=%ld rejected=%ld; restart passed.\n",
           (long)InterlockedCompareExchange(&stats.accepted, 0, 0),
           (long)InterlockedCompareExchange(&stats.rejected, 0, 0));
    return EXIT_SUCCESS;
}
