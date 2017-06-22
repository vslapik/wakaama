#include <pthread.h>
#include <unistd.h>
#include "poller.h"

typedef struct {
    pthread_t poller_thread;
    sqlite3 *db;
    uvector_t *sensors;
    pthread_mutex_t *lwm2m_lock;
    struct timeval timeout;
    int terminate_read_fd;
    int terminate_write_fd;
} poller_cfg_t;

poller_cfg_t poller_cfg;

void *poll_sensors(void *data)
{
    struct timeval timeout;
    poller_cfg_t *cfg = data;
    fd_set set;
    int ret;

    for (;;)
    {
        // poll data here

        // must re-setup all select() params for each call
        FD_ZERO(&set);
        FD_SET(cfg->terminate_read_fd, &set);
        timeout = cfg->timeout;
        ret = select(cfg->terminate_read_fd + 1, &set, NULL, NULL, &timeout);
        if (ret != 0) // 0 means timeout expired
        {
            UASSERT_PERROR(ret != -1); // error
            if (ret == 1) // some data appeared in the channedl, just exit
            {
                break;
            }
        }
    }
}

void start_poller(uvector_t *sensors, sqlite3 *db, pthread_mutex_t *lwm2m_lock,
                  int interval)
{

    int terminate_pipe[2];
    UASSERT_PERROR(pipe(terminate_pipe) != -1);

    poller_cfg.terminate_read_fd = terminate_pipe[0];
    poller_cfg.terminate_write_fd = terminate_pipe[1];
    poller_cfg.db = db;
    poller_cfg.sensors = sensors;
    poller_cfg.lwm2m_lock = lwm2m_lock;
    poller_cfg.timeout.tv_sec = interval;
    poller_cfg.timeout.tv_usec = 0;

    UASSERT_PERROR(pthread_create(&poller_cfg.poller_thread, NULL, poll_sensors, &poller_cfg) == 0);

    char *sensors_str = uvector_as_str(sensors);
    printf("polling thread was created to gather values "
           "from the following resources: %s\n", sensors_str);
    ufree(sensors_str);
}

void stop_poller(void)
{
    UASSERT_PERROR(write(poller_cfg.terminate_write_fd, "stop", 5) == 5);
    UASSERT_PERROR(pthread_join(poller_cfg.poller_thread, NULL) == 0);

    printf("polling thread terminated\n");
}
