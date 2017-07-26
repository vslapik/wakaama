#include <ugeneric.h>
#include <unistd.h>
#include "glue.h"

data_consumer_t *data_consumer_create(struct timeval timeout, int terminate_fd)
{
    data_consumer_t *dc = umalloc(sizeof(data_consumer_t));
    dc->data_available = false;
    dc->data = NULL;
    dc->data_size = 0;
    dc->timeout = timeout;
    dc->terminate_fd = terminate_fd;

    int data_pipe[2];
    UASSERT_PERROR(pipe(data_pipe) != -1);
    dc->data_read_fd = data_pipe[0];
    dc->data_write_fd = data_pipe[1];

    return dc;
}

void data_consumer_put_data(data_consumer_t *dc, void *data, size_t data_size)
{
    dc->data = data;
    dc->data_size = data_size;
    dc->data_available = true;

    UASSERT_PERROR(write(dc->data_write_fd, "ready", sizeof("ready") == sizeof("ready")));
}

void data_consumer_wait_for_data(data_consumer_t *dc)
{
    fd_set set;
    FD_ZERO(&set);
    FD_SET(dc->terminate_fd, &set);
    FD_SET(dc->data_read_fd, &set);
    struct timeval timeout = dc->timeout;
    int ret = select(MAX(dc->data_read_fd, dc->terminate_fd) + 1, &set, NULL, NULL, &timeout);
    UASSERT_PERROR(ret != -1); // error
    if (ret == 0)
    {
        UABORT("lwm2m code didn't return data"); // TODO: patch the lwm2m library to be able to configure CoAP timeouts
    }
    else
    {
        if (FD_ISSET(dc->terminate_fd, &set))
        {
            printf("terminating data consumer\n");
        }
        else if (FD_ISSET(dc->data_read_fd, &set))
        {
            // That's what we want.
//            printf("data arrived\n");
        }
        else
        {
            UABORT("select returned crap");
        }
    }
}

void data_consumer_destroy(data_consumer_t *dc)
{
    close(dc->data_read_fd);
    close(dc->data_write_fd);
    ufree(dc);
}

static void read_callback(uint16_t clientID, lwm2m_uri_t *uri,
                          int status, lwm2m_media_type_t fmt,
                          uint8_t *data, int data_size, void *user_data)
{
    data_consumer_t *dc = user_data;
    lwm2m_read_response_t *resp = umalloc(sizeof(lwm2m_read_response_t) + data_size + 1);
    resp->clientID = clientID;
    memcpy(&resp->uri, uri, sizeof(uri));
    resp->status = status;
    resp->fmt = fmt;
    resp->data_size = data_size;
    memcpy(resp->data, data, data_size);
    resp->data[data_size] = 0; // null-terminate payload

    data_consumer_put_data(dc, resp, data_size);
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
                      int terminate_fd,
                      lwm2m_read_response_t **response, size_t *response_size)
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

    // Timeout for response is hardcoded in transaction.c and there is no
    // way to configure it. Timeout should be greater than CoAP timeout. If
    // data consumer timeouts it means we are screwed as lwm2m library
    // neither signalled response timeout nor  reply received and likely
    // the code got stuck somewhere, not expected to happen under normal conditions.
    dc = data_consumer_create((struct timeval){.tv_sec = 120}, terminate_fd);

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
        *response = dc->data;
        *response_size = dc->data_size;
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

const char *status_to_str(int status)
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
