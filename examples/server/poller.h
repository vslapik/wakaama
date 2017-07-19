#ifndef __POLLER_H
#define __POLLER_H

#include <signal.h>
#include <liblwm2m.h>
#include <ugeneric.h>
#include "db.h"

typedef struct poller_opaq poller_t;
poller_t *poller_create(const char *device_id, const char *sensors_str, sqlite3 *db, lwm2m_context_t *lwm2m_ctx,
                        pthread_mutex_t *lwm2m_lock, int interval);
void poller_destroy(poller_t *p);
void poller_start(poller_t *p);
void poller_stop(poller_t *p);

#endif
