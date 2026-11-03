
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "audit_worker.h"
#include "audit_log.h"

#define AUDIT_QUEUE_CAPACITY 128
#define AUDIT_ACTION_MAX 64
#define AUDIT_DETAILS_MAX 256

typedef struct {
    char username[USERNAME_MAX];
    char action[AUDIT_ACTION_MAX];
    char details[AUDIT_DETAILS_MAX];
    AppStatus result;
} AuditEvent;

/* Bounded circular queue */
static AuditEvent eventQueue[AUDIT_QUEUE_CAPACITY];

static size_t queueHead = 0;
static size_t queueTail = 0;
static size_t queueCount = 0;

/* Worker lifecycle state */
static int workerRunning = 0;
static int workerStopping = 0;

/* Thread handles */
static pthread_t workerThread;

/*
 * stateLock protects:
 * queueHead, queueTail, queueCount,
 * workerRunning, workerStopping and eventQueue.
 */
static pthread_mutex_t stateLock = PTHREAD_MUTEX_INITIALIZER;

/* Producer/worker coordination */
static pthread_cond_t queueNotEmpty = PTHREAD_COND_INITIALIZER;
static pthread_cond_t queueNotFull = PTHREAD_COND_INITIALIZER;

/*
 * Serializes start and stop operations.
 * It prevents two callers from joining or starting
 * the same worker simultaneously.
 */
static pthread_mutex_t lifecycleLock = PTHREAD_MUTEX_INITIALIZER;


static void copyText(char *destination,
                     size_t capacity,
                     const char *source)
{
    if (capacity == 0) {
        return;
    }

    if (source == NULL) {
        source = "";
    }

    (void)snprintf(destination, capacity, "%s", source);
    destination[capacity - 1] = '\0';
}


/* Background worker thread */
static void *auditWorkerMain(void *unused)
{
    (void)unused;

    for (;;) {
        AuditEvent event;
        AppStatus writeStatus;

        pthread_mutex_lock(&stateLock);

        /* Wait until an event arrives or shutdown begins. */
        while (queueCount == 0 && !workerStopping) {
            pthread_cond_wait(&queueNotEmpty, &stateLock);
        }

        /*
         * During shutdown, finish processing all queued events
         * before terminating.
         */
        if (queueCount == 0 && workerStopping) {
            pthread_mutex_unlock(&stateLock);
            break;
        }

        /* Remove one event from the circular queue. */
        event = eventQueue[queueHead];

        queueHead = (queueHead + 1) % AUDIT_QUEUE_CAPACITY;
        --queueCount;

        /* Notify one waiting producer that space is available. */
        pthread_cond_signal(&queueNotFull);

        pthread_mutex_unlock(&stateLock);

        /* Perform file I/O outside the queue mutex. */
        writeStatus = writeAuditLog(
            event.username,
            event.action,
            event.details,
            event.result
        );

        if (writeStatus != APP_SUCCESS) {
            (void)fprintf(
                stderr,
                "Warning: background audit write failed.\n"
            );
        }
    }

    return NULL;
}


/* Start the background audit worker */
AppStatus startAuditWorker(void)
{
    int threadStatus;

    pthread_mutex_lock(&lifecycleLock);
    pthread_mutex_lock(&stateLock);

    if (workerRunning || workerStopping) {
        pthread_mutex_unlock(&stateLock);
        pthread_mutex_unlock(&lifecycleLock);
        return APP_DUPLICATE;
    }

    queueHead = 0;
    queueTail = 0;
    queueCount = 0;

    workerStopping = 0;

    pthread_mutex_unlock(&stateLock);

    threadStatus = pthread_create(
        &workerThread,
        NULL,
        auditWorkerMain,
        NULL
    );

    if (threadStatus != 0) {
        pthread_mutex_unlock(&lifecycleLock);
        return APP_ERROR;
    }

    pthread_mutex_lock(&stateLock);
    workerRunning = 1;
    pthread_mutex_unlock(&stateLock);

    pthread_mutex_unlock(&lifecycleLock);

    return APP_SUCCESS;
}


/* Add an event to the bounded queue */
AppStatus queueAuditEvent(
    const char *username,
    const char *action,
    const char *details,
    AppStatus result)
{
    if (username == NULL || action == NULL || details == NULL) {
        return APP_INVALID_INPUT;
    }

    pthread_mutex_lock(&stateLock);

    if (!workerRunning || workerStopping) {
        pthread_mutex_unlock(&stateLock);
        return APP_ERROR;
    }

    /*
     * If the queue is full, wait until the worker removes
     * an event or shutdown begins.
     */
    while (queueCount == AUDIT_QUEUE_CAPACITY &&
           !workerStopping) {

        pthread_cond_wait(&queueNotFull, &stateLock);
    }

    /* Reject events if shutdown has started. */
    if (!workerRunning || workerStopping) {
        pthread_mutex_unlock(&stateLock);
        return APP_ERROR;
    }

    /* Copy event data into the queue. */
    copyText(
        eventQueue[queueTail].username,
        sizeof(eventQueue[queueTail].username),
        username
    );

    copyText(
        eventQueue[queueTail].action,
        sizeof(eventQueue[queueTail].action),
        action
    );

    copyText(
        eventQueue[queueTail].details,
        sizeof(eventQueue[queueTail].details),
        details
    );

    eventQueue[queueTail].result = result;

    queueTail = (queueTail + 1) % AUDIT_QUEUE_CAPACITY;
    ++queueCount;

    /* Wake the background worker. */
    pthread_cond_signal(&queueNotEmpty);

    pthread_mutex_unlock(&stateLock);

    return APP_SUCCESS;
}


/* Stop worker after draining pending events */
void stopAuditWorker(void)
{
    pthread_mutex_lock(&lifecycleLock);
    pthread_mutex_lock(&stateLock);

    if (!workerRunning) {
        pthread_mutex_unlock(&stateLock);
        pthread_mutex_unlock(&lifecycleLock);
        return;
    }

    workerStopping = 1;

    /*
     * Wake the worker and all producers waiting for queue space.
     */
    pthread_cond_broadcast(&queueNotEmpty);
    pthread_cond_broadcast(&queueNotFull);

    pthread_mutex_unlock(&stateLock);

    /* Wait until the worker finishes processing pending events. */
    pthread_join(workerThread, NULL);

    pthread_mutex_lock(&stateLock);

    workerRunning = 0;
    workerStopping = 0;

    queueHead = 0;
    queueTail = 0;
    queueCount = 0;

    pthread_mutex_unlock(&stateLock);
    pthread_mutex_unlock(&lifecycleLock);
}
