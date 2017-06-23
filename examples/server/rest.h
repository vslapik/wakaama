#ifndef __REST_H
#define __REST_H

#include <liblwm2m.h>
#include <microhttpd.h>
#include <pthread.h>
#include "db.h"

void start_httpd(int port, lwm2m_context_t *lwm2m_ctx, pthread_mutex_t *lwm2m_lock, sqlite3 *db);
void stop_httpd(void);

#endif
