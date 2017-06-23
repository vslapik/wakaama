#include <pthread.h>
#include <unistd.h>
#include "poller.h"
#include "glue.h"

typedef struct {
    pthread_t poller_thread;
    sqlite3 *db;
    uvector_t *sensors;
    pthread_mutex_t *lwm2m_lock;
    lwm2m_context_t *lwm2m_ctx;
    struct timeval timeout;
    int terminate_read_fd;
    int terminate_write_fd;
} poller_cfg_t;
static poller_cfg_t g_poller_cfg;

static void poll_sensors(poller_cfg_t *cfg)
{
    uvector_t *devices = lwm2m_get_devices(cfg->lwm2m_ctx, cfg->lwm2m_lock);
    ugeneric_t *d = uvector_get_cells(devices);
    ugeneric_t *s = uvector_get_cells(cfg->sensors);
    size_t dsize = uvector_get_size(devices);
    size_t ssize = uvector_get_size(cfg->sensors);

    for (size_t i = 0; i < dsize; i++)
    {
        for (size_t j = 0; j < ssize; j++)
        {
            char *response;
            size_t response_size;
            printf("poll %s/%s\n", G_AS_STR(d[i]), G_AS_STR(s[j]));
            if (lwm2m_read_sensor(G_AS_STR(d[i]), G_AS_STR(s[j]), cfg->lwm2m_ctx, cfg->lwm2m_lock, &response, &response_size) == 0)
            {
                if (response_size)
                {
                    // consume data
                    ufree(response);
                }
            }
            else
            {
                // don't care, if there is anythin to consume we consume, it not - bail out
            }
        }
    }

    uvector_destroy(devices);
}

static void *loop(void *data)
{
    struct timeval timeout;
    poller_cfg_t *cfg = data;
    fd_set set;
    int ret;

    for (;;)
    {
        poll_sensors(cfg);

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

void start_poller(uvector_t *sensors, sqlite3 *db, lwm2m_context_t *lwm2m_ctx,
                  pthread_mutex_t *lwm2m_lock, int interval)
{

    int terminate_pipe[2];
    UASSERT_PERROR(pipe(terminate_pipe) != -1);

    g_poller_cfg.terminate_read_fd = terminate_pipe[0];
    g_poller_cfg.terminate_write_fd = terminate_pipe[1];
    g_poller_cfg.db = db;
    g_poller_cfg.sensors = sensors;
    g_poller_cfg.lwm2m_lock = lwm2m_lock;
    g_poller_cfg.lwm2m_ctx = lwm2m_ctx;
    g_poller_cfg.timeout.tv_sec = interval;
    g_poller_cfg.timeout.tv_usec = 0;

    UASSERT_PERROR(pthread_create(&g_poller_cfg.poller_thread, NULL, loop, &g_poller_cfg) == 0);

    char *sensors_str = uvector_as_str(sensors);
    printf("polling thread was created to gather values "
           "from the following resources: %s\n", sensors_str);
    ufree(sensors_str);
}

void stop_poller(void)
{
    UASSERT_PERROR(write(g_poller_cfg.terminate_write_fd, "stop", 5) == 5);
    UASSERT_PERROR(pthread_join(g_poller_cfg.poller_thread, NULL) == 0);

    printf("polling thread terminated\n");
}
