#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include "poller.h"
#include "db.h"
#include "glue.h"

struct poller_opaq {
    pthread_t poller_thread;
    sqlite3 *db;
    uvector_t *sensors;
    pthread_mutex_t *lwm2m_lock;
    lwm2m_context_t *lwm2m_ctx;
    struct timeval interval;
    int terminate_read_fd;
    int terminate_write_fd;
    char *device_id;
};

static char *extract_sample(const lwm2m_read_response_t *response)
{
    if (response->status == COAP_205_CONTENT)
    {
        UASSERT(response->fmt == LWM2M_CONTENT_JSON);
    }
    else
    {
        fprintf(stderr, "device replied with error: %s\n", status_to_str(response->status));
        return NULL;
    }

    ugeneric_t g = ugeneric_parse(response->data);

    if (G_IS_ERROR(g))
    {
        ugeneric_error_print(g);
        ugeneric_error_destroy(g);
        return NULL;
    }

    ugeneric_t e = udict_get(G_AS_PTR(g), G_STR("e"), G_NULL);
    if (G_IS_NULL(e) || !G_IS_VECTOR(e) || (uvector_get_size(G_AS_PTR(e)) != 1))
    {
        goto invalid_json;
    }

    ugeneric_t vd = uvector_get_at(G_AS_PTR(e), 0);
    if (!G_IS_DICT(vd))
    {
        goto invalid_json;
    }

    ugeneric_t v = udict_get(G_AS_PTR(vd), G_STR("v"), G_NULL);
    if (G_IS_NULL(v))
    {
        goto invalid_json;
    }

    char *sample = ugeneric_as_str(v);
    ugeneric_destroy(g, NULL);
    return sample;

invalid_json:
    ugeneric_destroy(g, NULL);
    return NULL;
}

static void poller_poll(poller_t *p)
{
    UASSERT_INPUT(p);

    ugeneric_t *s = uvector_get_cells(p->sensors);
    size_t len = uvector_get_size(p->sensors);

    for (size_t j = 0; j < len; j++)
    {
        lwm2m_read_response_t *response;
        size_t response_size;

        printf("poll %s/%s\n", p->device_id, G_AS_STR(s[j]));
        if (lwm2m_read_sensor(p->device_id, G_AS_STR(s[j]), p->lwm2m_ctx, p->lwm2m_lock, p->terminate_read_fd, &response, &response_size) == 0)
        {
            if (response_size)
            {
                char *sample = extract_sample(response);
                if (sample)
                {
                    save_sensor(p->db, p->device_id, G_AS_STR(s[j]), "TBD[name]", "TBD[unit]");
                    insert_sample(p->db, p->device_id, G_AS_STR(s[j]), sample, time(NULL));
                    ufree(sample);
                }
                ufree(response);
            }
        }
        else
        {
            // don't care, if there is anything to consume we consume, it not - bail out
            fprintf(stderr, "lwm2m_read_sensor() failed\n");
        }
    }
}

static void *loop(void *data)
{
    struct timeval timeout;
    poller_t *p = data;
    fd_set set;
    int ret;

    for (;;)
    {
        poller_poll(p);

        // must re-setup all select() params for each call
        FD_ZERO(&set);
        FD_SET(p->terminate_read_fd, &set);
        timeout = p->interval;
        ret = select(p->terminate_read_fd + 1, &set, NULL, NULL, &timeout);
        if (ret != 0) // 0 means timeout expired
        {
            UASSERT_PERROR(ret != -1); // error
            if (ret == 1) // some data appeared in the terminate channel, exit from loop
            {
                break;
            }
            else
            {
                UABORT("select returned unexpected value");
            }
        }
    }
}

poller_t *poller_create(const char *device_id, const char *sensors_str, sqlite3 *db, lwm2m_context_t *lwm2m_ctx,
                        pthread_mutex_t *lwm2m_lock, int interval)
{
    UASSERT_INPUT(sensors_str);
    UASSERT_INPUT(db);
    UASSERT_INPUT(lwm2m_ctx);
    UASSERT_INPUT(lwm2m_lock);
    UASSERT_INPUT(interval > 0);

    int terminate_pipe[2];
    UASSERT_PERROR(pipe(terminate_pipe) != -1);

    poller_t *p = umalloc(sizeof(*p));
    p->terminate_read_fd = terminate_pipe[0];
    p->terminate_write_fd = terminate_pipe[1];
    p->device_id = ustring_dup(device_id);
    p->db = db;
    p->lwm2m_lock = lwm2m_lock;
    p->lwm2m_ctx = lwm2m_ctx;
    p->interval.tv_sec = interval;
    p->interval.tv_usec = 0;
    p->sensors = ustring_split(sensors_str, ",");

    return p;
}

void poller_start(poller_t *p)
{
    UASSERT_INPUT(p);

    UASSERT_PERROR(pthread_create(&p->poller_thread, NULL, loop, p) == 0);

    char *sensors_str = uvector_as_str(p->sensors);
    printf("polling thread was created to gather values from the following resources %s on device %s.\n", sensors_str, p->device_id);
    ufree(sensors_str);
}

void poller_destroy(poller_t *p)
{
    if (p)
    {
        UASSERT_PERROR(write(p->terminate_write_fd, "stop", 5) == 5);
        UASSERT_PERROR(pthread_join(p->poller_thread, NULL) == 0);
        printf("polling thread for %s terminated\n", p->device_id);
        uvector_destroy(p->sensors);
        ufree(p->device_id);
        ufree(p);
    }
}
