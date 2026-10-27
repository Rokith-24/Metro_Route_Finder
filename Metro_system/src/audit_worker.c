#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "audit_worker.h"
#include "audit_log.h"

#define AUDIT_QUEUE_CAPACITY 128
#define AUDIT_ACTION_MAX 64
#define AUDIT_DETAILS_MAX 256
#define PRODUCER_WAIT_MS 50

typedef struct {
    char username[USERNAME_MAX];
    char action[AUDIT_ACTION_MAX];
    char details[AUDIT_DETAILS_MAX];
    AppStatus result;
} AuditEvent;

static AuditEvent eventQueue[AUDIT_QUEUE_CAPACITY];
static size_t queueHead = 0;
static size_t queueTail = 0;
static size_t queueCount = 0;
static int workerRunning = 0;
static int workerStopping = 0;
static unsigned long activeProducers = 0;
static HANDLE workerThread = NULL;
static HANDLE queueItems = NULL;
static HANDLE queueSlots = NULL;
static CRITICAL_SECTION queueLock;

/* This lock is initialized once and intentionally remains alive for process lifetime. */
static CRITICAL_SECTION lifecycleLock;
static volatile LONG lifecycleInitState = 0;

static void ensureLifecycleLock(void)
{
    LONG state = InterlockedCompareExchange(&lifecycleInitState, 1, 0);
    if (state == 0) {
        InitializeCriticalSection(&lifecycleLock);
        InterlockedExchange(&lifecycleInitState, 2);
    } else {
        while (InterlockedCompareExchange(&lifecycleInitState, 2, 2) != 2) {
            Sleep(0);
        }
    }
}

static void copyText(char *destination, size_t capacity, const char *source)
{
    if (capacity == 0) return;
    if (source == NULL) source = "";
    (void)snprintf(destination, capacity, "%s", source);
    destination[capacity - 1] = '\0';
}

static DWORD WINAPI auditWorkerMain(LPVOID unused)
{
    (void)unused;
    for (;;) {
        AuditEvent event;
        AppStatus writeStatus;

        if (WaitForSingleObject(queueItems, INFINITE) != WAIT_OBJECT_0) {
            (void)fprintf(stderr, "Warning: audit worker semaphore wait failed.\n");
            break;
        }

        EnterCriticalSection(&queueLock);
        if (queueCount == 0 && workerStopping) {
            LeaveCriticalSection(&queueLock);
            break;
        }
        if (queueCount == 0) {
            LeaveCriticalSection(&queueLock);
            continue;
        }

        event = eventQueue[queueHead];
        queueHead = (queueHead + 1) % AUDIT_QUEUE_CAPACITY;
        --queueCount;
        LeaveCriticalSection(&queueLock);

        (void)ReleaseSemaphore(queueSlots, 1, NULL);
        writeStatus = writeAuditLog(event.username, event.action,
                                    event.details, event.result);
        if (writeStatus != APP_SUCCESS) {
            (void)fprintf(stderr, "Warning: background audit write failed.\n");
        }
    }
    return 0;
}

AppStatus startAuditWorker(void)
{
    ensureLifecycleLock();
    EnterCriticalSection(&lifecycleLock);
    if (workerRunning || workerThread != NULL) {
        LeaveCriticalSection(&lifecycleLock);
        return APP_DUPLICATE;
    }

    queueHead = 0;
    queueTail = 0;
    queueCount = 0;
    workerStopping = 0;
    activeProducers = 0;
    InitializeCriticalSection(&queueLock);

    queueItems = CreateSemaphoreA(NULL, 0, LONG_MAX, NULL);
    queueSlots = CreateSemaphoreA(NULL, AUDIT_QUEUE_CAPACITY, AUDIT_QUEUE_CAPACITY, NULL);
    if (queueItems == NULL || queueSlots == NULL) {
        if (queueItems != NULL) CloseHandle(queueItems);
        if (queueSlots != NULL) CloseHandle(queueSlots);
        queueItems = NULL;
        queueSlots = NULL;
        DeleteCriticalSection(&queueLock);
        LeaveCriticalSection(&lifecycleLock);
        return APP_ERROR;
    }

    workerThread = CreateThread(NULL, 0, auditWorkerMain, NULL, 0, NULL);
    if (workerThread == NULL) {
        CloseHandle(queueItems);
        CloseHandle(queueSlots);
        queueItems = NULL;
        queueSlots = NULL;
        DeleteCriticalSection(&queueLock);
        LeaveCriticalSection(&lifecycleLock);
        return APP_ERROR;
    }
    workerRunning = 1;
    LeaveCriticalSection(&lifecycleLock);
    return APP_SUCCESS;
}

AppStatus queueAuditEvent(const char *username, const char *action,
                          const char *details, AppStatus result)
{
    HANDLE slots;
    HANDLE items;
    DWORD waitResult;

    if (username == NULL || action == NULL || details == NULL)
        return APP_INVALID_INPUT;

    ensureLifecycleLock();
    EnterCriticalSection(&lifecycleLock);
    if (!workerRunning || workerStopping || queueSlots == NULL || queueItems == NULL) {
        LeaveCriticalSection(&lifecycleLock);
        return APP_ERROR;
    }
    ++activeProducers;
    slots = queueSlots;
    items = queueItems;
    LeaveCriticalSection(&lifecycleLock);

    for (;;) {
        waitResult = WaitForSingleObject(slots, PRODUCER_WAIT_MS);
        if (waitResult == WAIT_OBJECT_0) break;
        if (waitResult != WAIT_TIMEOUT) {
            EnterCriticalSection(&lifecycleLock);
            --activeProducers;
            LeaveCriticalSection(&lifecycleLock);
            return APP_ERROR;
        }
        EnterCriticalSection(&lifecycleLock);
        if (workerStopping || !workerRunning) {
            --activeProducers;
            LeaveCriticalSection(&lifecycleLock);
            return APP_ERROR;
        }
        LeaveCriticalSection(&lifecycleLock);
    }

    EnterCriticalSection(&lifecycleLock);
    if (workerStopping || !workerRunning) {
        --activeProducers;
        LeaveCriticalSection(&lifecycleLock);
        return APP_ERROR;
    }

    EnterCriticalSection(&queueLock);
    copyText(eventQueue[queueTail].username,
             sizeof(eventQueue[queueTail].username), username);
    copyText(eventQueue[queueTail].action,
             sizeof(eventQueue[queueTail].action), action);
    copyText(eventQueue[queueTail].details,
             sizeof(eventQueue[queueTail].details), details);
    eventQueue[queueTail].result = result;
    queueTail = (queueTail + 1) % AUDIT_QUEUE_CAPACITY;
    ++queueCount;
    LeaveCriticalSection(&queueLock);

    /*
     * Notify the worker before marking this producer inactive.
     * Shutdown must not proceed while a producer has queued an
     * event but has not yet signalled the worker.
     */
    if (!ReleaseSemaphore(items, 1, NULL)) {
        --activeProducers;
        LeaveCriticalSection(&lifecycleLock);
        (void)fprintf(stderr, "Warning: failed to signal audit worker.\n");
        return APP_ERROR;
    }

    --activeProducers;
    LeaveCriticalSection(&lifecycleLock);

    return APP_SUCCESS;
}

void stopAuditWorker(void)
{
    HANDLE threadToJoin;

    ensureLifecycleLock();
    EnterCriticalSection(&lifecycleLock);
    if (!workerRunning) {
        LeaveCriticalSection(&lifecycleLock);
        return;
    }
    workerStopping = 1;
    EnterCriticalSection(&queueLock);
    /* The worker drains queued items before exiting. */
    LeaveCriticalSection(&queueLock);
    threadToJoin = workerThread;
    (void)ReleaseSemaphore(queueItems, 1, NULL);
    LeaveCriticalSection(&lifecycleLock);

    /* Producers poll shutdown while waiting for a queue slot. Do not close
       handles or destroy queueLock until every in-flight producer has exited. */
    for (;;) {
        unsigned long producers;
        EnterCriticalSection(&lifecycleLock);
        producers = activeProducers;
        LeaveCriticalSection(&lifecycleLock);
        if (producers == 0) break;
        Sleep(1);
    }

    if (threadToJoin != NULL) {
        (void)WaitForSingleObject(threadToJoin, INFINITE);
        CloseHandle(threadToJoin);
    }

    EnterCriticalSection(&lifecycleLock);
    workerThread = NULL;
    workerRunning = 0;
    queueHead = queueTail = queueCount = 0;
    workerStopping = 0;
    if (queueItems != NULL) CloseHandle(queueItems);
    if (queueSlots != NULL) CloseHandle(queueSlots);
    queueItems = NULL;
    queueSlots = NULL;
    DeleteCriticalSection(&queueLock);
    LeaveCriticalSection(&lifecycleLock);
}
