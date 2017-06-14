#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "liblwm2m.h"
#include "rest.h"
#include "ugeneric.h"

#define CODE_TO_STRING(X)   case X : return #X

static const char *status_to_str(int status)
{
    switch(status)
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

static char *ustring_replace_char(const char *str, char from, char to)
{
    char *t, *out = NULL;

    if (str)
    {
        t = out = ustring_dup(str);
        while (*t)
        {
            if (*t == from)
            {
                *t = to;
            }
            t++;
        }
    }

    return out;
}

struct MHD_Daemon *g_httpd;
lwm2m_context_t *g_lwm2m_ctx;
pthread_mutex_t *g_lwm2m_lock;

static int handler(void *cls, struct MHD_Connection *cn, const char *url,
                   const char *method, const char *version,
                   const char *upload_data, size_t *upload_data_size, void **ptr);
static int respond_from_buffer(struct MHD_Connection *cn, const char *buffer, size_t buffer_size);
static int respond_404(struct MHD_Connection *cn);
static void output_sensor_value(uint16_t clientID,
                                lwm2m_uri_t *uri,
                                int status,
                                lwm2m_media_type_t fmt,
                                uint8_t *data,
                                int dataLength,
                                void *userData);

void start_httpd(lwm2m_context_t *lwm2m_ctx, pthread_mutex_t *lwm2m_lock)
{
    UASSERT(lwm2m_ctx);
    g_lwm2m_ctx = lwm2m_ctx;
    g_lwm2m_lock = lwm2m_lock;

    g_httpd = MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION,
            8888, NULL, NULL, &handler, NULL,
            MHD_OPTION_END);
    UASSERT(g_httpd);
}

static int handle_url(struct MHD_Connection *cn, const char *url, const uvector_t *parsed_url, const char *method);
static int get_devices_list(struct MHD_Connection *cn);
static int get_device_info(struct MHD_Connection *cn, const char *device_id);
static int get_sensors_list(struct MHD_Connection *cn, const char *device_id);
static int get_sensor_value(struct MHD_Connection *cn, const char *device_id, const char *sensor_id);
static lwm2m_client_t *find_device_by_id(const char *device_id);

static int respond_from_buffer(struct MHD_Connection *con, const char *buffer, size_t buffer_size)
{
    int ret;
    struct MHD_Response *response;
    response = MHD_create_response_from_buffer(buffer_size, (void *)buffer,
                                               MHD_RESPMEM_MUST_COPY);
    ret = MHD_queue_response(con, MHD_HTTP_OK, response);
    MHD_destroy_response(response);

    return ret;
}

static int respond_404(struct MHD_Connection *con)
{
    int ret;
    struct MHD_Response *response;
    const char *NOT_FOUND_ERROR = "<html>"
                                  "<head><title>Not found</title></head>"
                                  "<body>Go away.</body>"
                                  "</html>";
    response = MHD_create_response_from_buffer(strlen(NOT_FOUND_ERROR),
                                               (void *)NOT_FOUND_ERROR,
                                               MHD_RESPMEM_PERSISTENT);
    ret = MHD_queue_response(con, MHD_HTTP_NOT_FOUND, response);
    MHD_destroy_response(response);

    return ret;
}

static int handler(void *cls, struct MHD_Connection *cn, const char *url,
                   const char *method, const char *version,
                   const char *upload_data, size_t *upload_data_size, void **ptr)
{
    static int dummy;
    const char *page = cls;
    struct MHD_Response *response;
    int ret;

    if ((0 != strcmp(method, "GET")) &&
        (0 != strcmp(method, "POST")))
    {
        return MHD_NO;
    }

    if (&dummy != *ptr)
    {
        /* The first time only the headers are valid,
           do not respond in the first round... */
        *ptr = &dummy;
        return MHD_YES;
    }

    if (0 != *upload_data_size)
    {
        return MHD_NO; /* upload data in a GET!? */
    }

    *ptr = NULL; /* clear context pointer */

    if (strlen(url) == 0)
    {
        return respond_404(cn);
    }
    if (url[0] != '/')
    {
        return respond_404(cn);
    }

    uvector_t *v = ustring_split(url + 1, "/");
    ret = handle_url(cn, url, v, method);
    uvector_destroy(v);

    return ret;
}

static int handle_url(struct MHD_Connection *cn, const char *url, const uvector_t *parsed_url, const char *method)
{
    if (uvector_is_empty(parsed_url))
    {
        return respond_404(cn);
    }

    char *top = G_AS_STR(uvector_get_at(parsed_url, 0));
    size_t url_components = uvector_get_size(parsed_url);

    if (strcmp(top, "devices") == 0)
    {
        /* /devices/list */
        if (url_components == 2)
        {
            if (strcmp(G_AS_STR(uvector_get_at(parsed_url, 1)), "list") == 0)
            {
                return get_devices_list(cn);
            }
        }
        /* /devices/{device_id}/sensors/list/ */
        else if (url_components == 4)
        {
            if ((strcmp(G_AS_STR(uvector_get_at(parsed_url, 2)), "sensors") == 0) &&
                (strcmp(G_AS_STR(uvector_get_at(parsed_url, 3)), "list") == 0))
            {
                const char *device_id = G_AS_STR(uvector_get_at(parsed_url, 1));
                return get_sensors_list(cn, device_id);
            }
        }
        /*
         * /devices/{device_id}/sensors/{sensor_id}/value
         * /devices/{device_id}/sensors/{sensor_id}/stat
         * /devices/{device_id}/sensors/{sensor_id}/value
         */
        else if (url_components == 5)
        {
            if (strcmp(G_AS_STR(uvector_get_at(parsed_url, 2)), "sensors") == 0)
            {
                const char *device_id = G_AS_STR(uvector_get_at(parsed_url, 1));
                const char *sensor_id = G_AS_STR(uvector_get_at(parsed_url, 3));
                const char *t = G_AS_STR(uvector_get_at(parsed_url, 4));
                if (strcmp(t, "value") == 0)
                {
                    if (method[0] == 'G')
                    {
                        return get_sensor_value(cn, device_id, sensor_id);
                    }
                    else
                    {
                        // set sensor value
                    }
                }
                else if (strcmp(t, "stat"))
                {
                    // return stat
                }
            }
        }
    }
    else if (strlen(url))
    {
        int ret;
        char *path = ustring_fmt("%s/%s", "www", url);
        ugeneric_t g = ufile_read_to_memchunk(path);
        ufree(path);
        if (G_IS_ERROR(g))
        {
            // ugeneric_error_print(g);
            // char *err = ugeneric_error_as_str(g);
            // ret = respond_from_buffer(cn, err, strlen(err));
            // ufree(err);
            ugeneric_error_destroy(g);
            return ret;
        }
        else
        {
            char *resp = G_AS_MEMCHUNK_DATA(g);
            ret = respond_from_buffer(cn, resp, G_AS_MEMCHUNK_SIZE(g));
            return ret;
        }
    }

    return respond_404(cn);
}

static lwm2m_client_t *find_device_by_id(const char *device_id)
{
    for (lwm2m_client_t *c = g_lwm2m_ctx->clientList; c; c = c->next)
    {
        if (strcmp(c->name, device_id) == 0)
        {
            return c;
        }
    }

    return NULL;
}

static int get_devices_list(struct MHD_Connection *cn)
{
    pthread_mutex_lock(g_lwm2m_lock);

    udict_t *td = udict_create();
    uvector_t *v = uvector_create();

    lwm2m_client_t *c = NULL;
    for (c = g_lwm2m_ctx->clientList; c; c = c->next)
    {
        uvector_append(v, G_STR(ustring_dup(c->name)));
    }
    udict_put(td, G_CSTR("data"), G_VECTOR(v));

    char *str = udict_as_str(td);
    int ret = respond_from_buffer(cn, str, strlen(str));
    udict_destroy(td);

    pthread_mutex_unlock(g_lwm2m_lock);
    return ret;
}

static int get_sensors_list(struct MHD_Connection *cn, const char *device_id)
{
    printf("Getting sensors list for %s\n", device_id);
    pthread_mutex_lock(g_lwm2m_lock);

    lwm2m_client_t *c = find_device_by_id(device_id);
    udict_t *d = udict_create();
    uvector_t *sensors = uvector_create();

    if (c)
    {
        for (lwm2m_client_object_t *obj = c->objectList; obj; obj = obj->next)
        {
            if (obj->instanceList == NULL)
            {
                //fprintf(stdout, "/%d, ", obj->id);
                //uvector_append(sensors, obj->id);
            }
            else
            {
                lwm2m_list_t *inst;
                for (inst = obj->instanceList; inst; inst = inst->next)
                {
                    uvector_append(sensors, G_STR(ustring_fmt(".%d.%d", obj->id, inst->id)));
                }
            }
        }
    }
    udict_put(d, G_CSTR("data"), G_VECTOR(sensors));

    char *str = udict_as_str(d);
    int ret = respond_from_buffer(cn, str, strlen(str));
    udict_destroy(d);
    ufree(str);

    pthread_mutex_unlock(g_lwm2m_lock);
    return ret;
}

static int get_sensor_value(struct MHD_Connection *cn, const char *device_id, const char *sensor_id)
{
    pthread_mutex_lock(g_lwm2m_lock);

    lwm2m_uri_t uri;
    int ret = MHD_YES;

    char *id = ustring_replace_char(sensor_id, '.', '/');
    printf("%s\n", id);
    int result = lwm2m_stringToUri(id, strlen(id), &uri);
    ufree(id);

    if (result == 0)
    {
        goto not_found;
    }

    lwm2m_client_t *c = find_device_by_id(device_id);
    if (!c)
    {
        goto not_found;
    }

    result = lwm2m_dm_read(g_lwm2m_ctx, c->internalID, &uri, output_sensor_value, cn);
    if (result != 0)
    {
        goto not_found;
    }
    goto found;

not_found:
    ret = respond_404(cn);

found:
    pthread_mutex_unlock(g_lwm2m_lock);
    return ret;
}

void stop_httpd(void)
{
    MHD_stop_daemon(g_httpd);
}

/*
 * Executed in context of lwm2m_step, already locked.
 */
static void output_sensor_value(uint16_t clientID,
                                lwm2m_uri_t *uri,
                                int status,
                                lwm2m_media_type_t fmt,
                                uint8_t *data,
                                int dataLength,
                                void *userData)
{
    struct MHD_Connection *cn = userData;

    if (status != COAP_NO_ERROR)
    {
        const char *err = status_to_str(status);
        respond_from_buffer(cn, err, strlen(err));
        return;
    }

    UASSERT(fmt == LWM2M_CONTENT_JSON);
    respond_from_buffer(cn, data, dataLength);
}

