#ifndef AUDIT_WORKER_H
#define AUDIT_WORKER_H

#include "common.h"

/* Starts one background worker that writes queued audit events to audit.log. */
AppStatus startAuditWorker(void);

/* Queues an audit event. Returns an error if the worker is not running. */
AppStatus queueAuditEvent(const char *username,
                          const char *action,
                          const char *details,
                          AppStatus result);

/* Stops the worker after it drains pending events. Safe to call if not started. */
void stopAuditWorker(void);

#endif
