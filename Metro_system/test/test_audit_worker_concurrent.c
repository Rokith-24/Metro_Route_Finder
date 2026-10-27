#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audit_worker.h"
#include "common.h"

#define PRODUCER_COUNT 4
#define EVENTS_PER_PRODUCER 20
#define TOTAL_EVENTS (PRODUCER_COUNT * EVENTS_PER_PRODUCER)

/* Each producer submits uniquely named events so the log can be verified. */
typedef struct {
    int producerId;
    int failed;
} ProducerArgs;

static DWORD WINAPI producerMain(LPVOID parameter)
{
    ProducerArgs *args = (ProducerArgs *)parameter;
    int i;

    for (i = 0; i < EVENTS_PER_PRODUCER; ++i) {
        char action[64];
        char details[128];
        AppStatus status;

        (void)snprintf(action, sizeof(action), "CONCURRENT_P%d_E%02d",
                       args->producerId, i);
        (void)snprintf(details, sizeof(details), "producer=%d event=%d",
                       args->producerId, i);

        status = queueAuditEvent("concurrency_test", action, details,
                                 APP_SUCCESS);
        if (status != APP_SUCCESS) {
            args->failed = 1;
            return 1;
        }
    }

    return 0;
}

static int countActionInLog(const char *filename, const char *action)
{
    char line[1024];
    FILE *file = fopen(filename, "r");
    int found = 0;

    if (file == NULL) return 0;
    while (fgets(line, sizeof(line), file) != NULL) {
        if (strstr(line, action) != NULL) ++found;
    }
    fclose(file);
    return found;
}

int main(void)
{
    HANDLE threads[PRODUCER_COUNT];
    ProducerArgs args[PRODUCER_COUNT];
    int i;
    int missing = 0;

    for (i = 0; i < PRODUCER_COUNT; ++i) threads[i] = NULL;

    if (startAuditWorker() != APP_SUCCESS) {
        fprintf(stderr, "FAIL: could not start audit worker.\n");
        return EXIT_FAILURE;
    }

    for (i = 0; i < PRODUCER_COUNT; ++i) {
        args[i].producerId = i;
        args[i].failed = 0;
        threads[i] = CreateThread(NULL, 0, producerMain, &args[i], 0, NULL);
        if (threads[i] == NULL) {
            fprintf(stderr, "FAIL: could not create producer %d.\n", i);
            /* Join producers already created before stopping the worker. */
            while (--i >= 0) {
                (void)WaitForSingleObject(threads[i], INFINITE);
                CloseHandle(threads[i]);
            }
            stopAuditWorker();
            return EXIT_FAILURE;
        }
    }

    for (i = 0; i < PRODUCER_COUNT; ++i) {
        if (WaitForSingleObject(threads[i], INFINITE) != WAIT_OBJECT_0) {
            fprintf(stderr, "FAIL: waiting for producer %d failed.\n", i);
            missing = 1;
        }
        CloseHandle(threads[i]);
        if (args[i].failed) {
            fprintf(stderr, "FAIL: producer %d could not queue all events.\n", i);
            missing = 1;
        }
    }

    /* Joining producers first makes this test specifically check concurrent
       queueing and drain-on-shutdown, not producer-vs-shutdown races. */
    stopAuditWorker();

    for (i = 0; i < PRODUCER_COUNT; ++i) {
        int j;
        for (j = 0; j < EVENTS_PER_PRODUCER; ++j) {
            char action[64];
            (void)snprintf(action, sizeof(action), "CONCURRENT_P%d_E%02d", i, j);
            if (countActionInLog(AUDIT_FILE, action) != 1) {
                fprintf(stderr, "FAIL: expected exactly one log entry for %s.\n", action);
                missing = 1;
            }
        }
    }

    if (missing) {
        fprintf(stderr, "Concurrent audit test failed; expected %d unique events.\n",
                TOTAL_EVENTS);
        return EXIT_FAILURE;
    }

    printf("Concurrent audit test passed: %d events from %d producer threads.\n",
           TOTAL_EVENTS, PRODUCER_COUNT);
    return EXIT_SUCCESS;
}
