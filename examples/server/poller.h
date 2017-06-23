#ifndef __POLLER_H
#define __POLLER_H

#include <signal.h>
#include <liblwm2m.h>
#include <ugeneric.h>
#include "db.h"

void start_poller(uvector_t *sensors, sqlite3 *db, lwm2m_context_t *lwm2m_ctx,
                  pthread_mutex_t *lwm2m_lock, int interval);
void stop_poller(void);

#endif
