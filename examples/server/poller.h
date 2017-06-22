#include <signal.h>
#include "db.h"
#include "ugeneric.h"

void start_poller(uvector_t *sensors, sqlite3 *db, pthread_mutex_t *lwm2m_lock,
                  int interval);
void stop_poller(void);
