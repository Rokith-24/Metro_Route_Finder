#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audit_worker.h"
#include "common.h"

static int fileContains(const char *filename, const char *needle)
{
    char line[1024];
    FILE *file = fopen(filename, "r");
    int found = 0;

    if (file == NULL) return 0;
    while (fgets(line, sizeof(line), file) != NULL) {
        if (strstr(line, needle) != NULL) {
            found = 1;
            break;
        }
    }
    fclose(file);
    return found;
}

int main(void)
{
    AppStatus status;

    /* Uses unique event names and preserves the existing audit log. */

    status = queueAuditEvent("tester", "BEFORE_START", "must fail", APP_SUCCESS);
    if (status == APP_SUCCESS) {
        fprintf(stderr, "Queue unexpectedly accepted an event before start.\n");
        return EXIT_FAILURE;
    }

    status = startAuditWorker();
    if (status != APP_SUCCESS) {
        fprintf(stderr, "Could not start audit worker.\n");
        return EXIT_FAILURE;
    }

    status = queueAuditEvent("tester", "AUDIT_WORKER_TEST_20261027_A", "first event", APP_SUCCESS);
    if (status != APP_SUCCESS) goto fail;
    status = queueAuditEvent("tester", "AUDIT_WORKER_TEST_20261027_B", "second event", APP_NOT_FOUND);
    if (status != APP_SUCCESS) goto fail;

    stopAuditWorker(); /* Must drain queued events before returning. */

    if (!fileContains(AUDIT_FILE, "AUDIT_WORKER_TEST_20261027_A") ||
        !fileContains(AUDIT_FILE, "AUDIT_WORKER_TEST_20261027_B")) {
        fprintf(stderr, "Queued events were not both found in %s.\n", AUDIT_FILE);
        return EXIT_FAILURE;
    }

    status = queueAuditEvent("tester", "AFTER_STOP", "must fail", APP_SUCCESS);
    if (status == APP_SUCCESS) {
        fprintf(stderr, "Queue unexpectedly accepted an event after stop.\n");
        return EXIT_FAILURE;
    }

    puts("Audit worker tests passed.");
    return EXIT_SUCCESS;

fail:
    stopAuditWorker();
    fprintf(stderr, "Could not queue audit test event.\n");
    return EXIT_FAILURE;
}
