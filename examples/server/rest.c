#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "liblwm2m.h"
#include "rest.h"
#include "ugeneric.h"

#define NOT_FOUND_ERROR "<html><head><title>Not found</title></head><body>Go away.</body></html>"

struct MHD_Daemon *g_httpd;
lwm2m_context_t *g_lwm2m_ctx;

static int handler(void *cls,
        struct MHD_Connection *connection,
        const char *url,
        const char *method,
        const char *version,
        const char *upload_data,
        size_t *upload_data_size,
        void **ptr);

void start_httpd(lwm2m_context_t *lwm2m_ctx)
{
    UASSERT(lwm2m_ctx);
    g_lwm2m_ctx = lwm2m_ctx;

    g_httpd = MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION,
            8888, NULL, NULL, &handler, NULL,
            MHD_OPTION_END);
    UASSERT(g_httpd);
}

static char *handle_url(const char *url, const uvector_t *parsed_url, const char *method);
static char *get_devices_list(void);
static char *get_device_info(const char *device_id);
static char *get_sensors_list(const char *device_id);
static char *get_sensor_value(const char *device_id, const char *sensor_id);
static lwm2m_client_t *find_device_by_id(const char *device_id);

static int handler(void *cls,
        struct MHD_Connection *connection,
        const char *url,
        const char *method,
        const char *version,
        const char *upload_data,
        size_t *upload_data_size,
        void **ptr)
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
        return MHD_NO;
    }
    if (url[0] != '/')
    {
        return MHD_NO;
    }

    uvector_t *v = ustring_split(url, "/");
    char *response_data = handle_url(url, v, method);
    if (response_data)
    {
        response = MHD_create_response_from_buffer(strlen(response_data), (void *)response_data, MHD_RESPMEM_MUST_COPY);
        ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
        MHD_destroy_response(response);
        ufree(response_data);
    }
    else
    {
        response = MHD_create_response_from_buffer(strlen(NOT_FOUND_ERROR), (void *)NOT_FOUND_ERROR,
                                                          MHD_RESPMEM_PERSISTENT);
        ret = MHD_queue_response(connection, MHD_HTTP_NOT_FOUND, response);
        MHD_destroy_response(response);
    }
    uvector_destroy(v);

    return ret;
}

static char *handle_url(const char *url, const uvector_t *parsed_url, const char *method)
{
    uvector_print(parsed_url);

    if (uvector_get_size(parsed_url) < 2)
    {
        return NULL;
    }

    char *top = G_AS_STR(uvector_get_at(parsed_url, 1));
    if (strcmp(top, "devices") == 0)
    {
        if (uvector_get_size(parsed_url) < 3)
        {
            return NULL;
        }
        if (strcmp(G_AS_STR(uvector_get_at(parsed_url, 2)), "list") == 0)
        {
            if (uvector_get_size(parsed_url) != 3)
            {
                return NULL;
            }
            return get_devices_list();
        }
        else
        {
            const char *device_id = G_AS_STR(uvector_get_at(parsed_url, 2));
            if (uvector_get_size(parsed_url) < 4)
            {
                return NULL;
            }
            if (strcmp(G_AS_STR(uvector_get_at(parsed_url, 3)), "sensors") == 0)
            {
                if (uvector_get_size(parsed_url) < 5)
                {
                    return NULL;
                }
                const char *sensor_id = G_AS_STR(uvector_get_at(parsed_url, 4));
                if (strcmp(sensor_id, "list") == 0)
                {
                    if (uvector_get_size(parsed_url) != 5)
                    {
                        return NULL;
                    }
                    return get_sensors_list(device_id);
                }
                else
                {
                    get_sensor_value(device_id, sensor_id);
                }
            }
        }
    }
    else if (strlen(url))
    {
        char *path = ustring_fmt("%s/%s", "www", url);
        ugeneric_t g = ufile_read_to_string(path);
        ufree(path);
        if (G_IS_ERROR(g))
        {
            ugeneric_error_print(g);
            return NULL;
        }
        return G_AS_STR(g);
    }
    else
    {
        return NULL;
    }
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

static char *get_devices_list(void)
{
    udict_t *td = udict_create();
    uvector_t *v = uvector_create();

    lwm2m_client_t *c = NULL;
    for (c = g_lwm2m_ctx->clientList; c; c = c->next)
    {
        uvector_append(v, G_STR(ustring_dup(c->name)));
    }
    udict_put(td, G_CSTR("data"), G_VECTOR(v));
    char *str = udict_as_str(td);
    udict_destroy(td);

    return str;
}

static char *get_sensors_list(const char *device_id)
{
    printf("Getting sensors list for %s\n", device_id);

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
                    //fprintf(stdout, "/%d/%d, ", obj->id, inst->id);
                    uvector_append(sensors, G_STR(ustring_fmt("%d.%d", obj->id, inst->id)));
                }
            }
        }
    }
    udict_put(d, G_CSTR("data"), G_VECTOR(sensors));
    char *str = udict_as_str(d);
    udict_destroy(d);

    return str;
}

static char *get_sensor_value(const char *device_id, const char *sensor_id)
{
    lwm2m_client_t *c = find_device_by_id(device_id);
    udict_t *d = udict_create();
    uvector_t *sensors = uvector_create();

    if (c)
    {
    }

    udict_put(d, G_CSTR("data"), G_VECTOR(sensors));
    char *str = udict_as_str(d);
    udict_destroy(d);

    return str;
}

void stop_httpd(void)
{
    MHD_stop_daemon(g_httpd);
}
