#include <ugeneric.h>
#include "glue.h"

data_consumer_t *data_consumer_create(struct timespec timeout)
{
    data_consumer_t *dc = umalloc(sizeof(data_consumer_t));
    pthread_mutex_init(&dc->data_lock, NULL);
    pthread_cond_init(&dc->data_wait, NULL);
    dc->data_available = false;
    dc->data = NULL;
    dc->data_size = 0;
    dc->timeout = timeout;

    return dc;
}

void data_consumer_put_data(data_consumer_t *dc, void *data, size_t data_size)
{
    pthread_mutex_lock(&dc->data_lock);
    dc->data = data;
    dc->data_size = data_size;
    dc->data_available = true;
    pthread_cond_signal(&dc->data_wait);
    pthread_mutex_unlock(&dc->data_lock);
}

void data_consumer_wait_for_data(data_consumer_t *dc)
{
    struct timespec till;
    clock_gettime(CLOCK_REALTIME, &till);
    till.tv_sec += dc->timeout.tv_sec;
    till.tv_nsec += dc->timeout.tv_nsec;

    pthread_mutex_lock(&dc->data_lock);
    while (!dc->data_available)
    {
        int ret = pthread_cond_timedwait(&dc->data_wait, &dc->data_lock, &till);
        if (ret == ETIMEDOUT)
        {
            printf("lwm2m device didn't reply within timeout\n");
            break;
        }
    }
    pthread_mutex_unlock(&dc->data_lock);
}

void data_consumer_destroy(data_consumer_t *dc)
{
    pthread_mutex_destroy(&dc->data_lock);
    pthread_cond_destroy(&dc->data_wait);
    ufree(dc);
}

static const char *status_to_str(int status)
{
    #define CODE_TO_STRING(X) case X : return #X

    switch (status)
    {
        CODE_TO_STRING(COAP_NO_ERROR);
        CODE_TO_STRING(COAP_IGNORE);
        CODE_TO_STRING(COAP_201_CREATED);
        CODE_TO_STRING(COAP_202_DELETED);
        CODE_TO_STRING(COAP_204_CHANGED);
        CODE_TO_STRING(COAP_205_CONTENT);
        CODE_TO_STRING(COAP_400_BAD_REQUEST);
        CODE_TO_STRING(COAP_401_UNAUTHORIZED);
        CODE_TO_STRING(COAP_404_NOT_FOUND);
        CODE_TO_STRING(COAP_405_METHOD_NOT_ALLOWED);
        CODE_TO_STRING(COAP_406_NOT_ACCEPTABLE);
        CODE_TO_STRING(COAP_500_INTERNAL_SERVER_ERROR);
        CODE_TO_STRING(COAP_501_NOT_IMPLEMENTED);
        CODE_TO_STRING(COAP_503_SERVICE_UNAVAILABLE);
        default: return "";
    }
}

static void read_callback(uint16_t clientID, lwm2m_uri_t *uri,
                          int status, lwm2m_media_type_t fmt,
                          uint8_t *data, int data_size, void *user_data)
{
    data_consumer_t *dc = user_data;

    if (status == COAP_205_CONTENT)
    {
        UASSERT(fmt == LWM2M_CONTENT_JSON);
        data_consumer_put_data(dc, ustring_ndup(data, data_size), data_size);
    }
    else
    {
        fprintf(stderr, "device replied with error: %s\n", status_to_str(status));
        data_consumer_put_data(dc, NULL, 0);
    }
}

lwm2m_client_t *find_device_by_id(const char *device_id,
                                  lwm2m_context_t *lwm2m_ctx,
                                  pthread_mutex_t *lwm2m_lock)
{
    for (lwm2m_client_t *c = lwm2m_ctx->clientList; c; c = c->next)
    {
        if (strcmp(c->name, device_id) == 0)
        {
            return c;
        }
    }

    return NULL;
}

uvector_t *lwm2m_get_devices(lwm2m_context_t *lwm2m_ctx, pthread_mutex_t *lwm2m_lock)
{
    uvector_t *v = uvector_create();
    pthread_mutex_lock(lwm2m_lock);
    for (lwm2m_client_t *c = lwm2m_ctx->clientList; c; c = c->next)
    {
        uvector_append(v, G_STR(ustring_dup(c->name)));
    }
    pthread_mutex_unlock(lwm2m_lock);

    return v;
}

int lwm2m_write_sensor(const char *device_id, const char *sensor_id,
                       lwm2m_context_t *lwm2m_ctx, pthread_mutex_t *lwm2m_lock,
                       char **data, size_t *data_size)
{
    
}

int lwm2m_read_sensor(const char *device_id, const char *sensor_id,
                      lwm2m_context_t *lwm2m_ctx, pthread_mutex_t *lwm2m_lock,
                      char **data, size_t *data_size)
{
    lwm2m_uri_t uri;
    data_consumer_t *dc = NULL;
    char *id = NULL;
    int ret = 0;

    id = ustring_replace_char(sensor_id, '.', '/');
    if (lwm2m_stringToUri(id, strlen(id), &uri) == 0)
    {
        ret = 1;
        goto not_found;
    }

    lwm2m_client_t *c = find_device_by_id(device_id, lwm2m_ctx, lwm2m_lock);
    if (!c)
    {
        ret = 1;
        goto not_found;
    }

    dc = data_consumer_create((struct timespec){.tv_sec = 2, .tv_nsec = 0});

    pthread_mutex_lock(lwm2m_lock); //---------------------------------------
    ret = lwm2m_dm_read(lwm2m_ctx, c->internalID, &uri, read_callback, dc);
    pthread_mutex_unlock(lwm2m_lock); // ------------------------------------

    if (ret != 0)
    {
        goto not_found;
    }

    data_consumer_wait_for_data(dc);

    if (dc->data)
    {
        *data = dc->data;
        *data_size = dc->data_size;
    }
    else
    {
        ret = 1;
        goto not_found;
    }

not_found:
    if (dc)
    {
        data_consumer_destroy(dc);
    }
    if (id)
    {
        ufree(id);
    }

    return ret;
}
