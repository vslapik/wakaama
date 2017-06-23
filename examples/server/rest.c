#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <liblwm2m.h>
#include <ugeneric.h>

#include "rest.h"
#include "glue.h"

#define CODE_TO_STRING(X)   case X : return #X

static void read_callback(uint16_t clientID, lwm2m_uri_t *uri,
                          int status, lwm2m_media_type_t fmt,
                          uint8_t *data, int data_size, void *user_data);

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

struct MHD_Daemon *g_httpd;
lwm2m_context_t *g_lwm2m_ctx;
pthread_mutex_t *g_lwm2m_lock;
sqlite3 *g_db;

static int handler(void *cls, struct MHD_Connection *cn, const char *url,
                   const char *method, const char *version,
                   const char *upload_data, size_t *upload_data_size, void **ptr);
static int respond_from_buffer(struct MHD_Connection *cn, const char *buffer, size_t buffer_size);
static int respond_404(struct MHD_Connection *cn, const char *msg);

void start_httpd(int port, lwm2m_context_t *lwm2m_ctx, pthread_mutex_t *lwm2m_lock, sqlite3 *db)
{
    UASSERT(lwm2m_ctx);
    g_lwm2m_ctx = lwm2m_ctx;
    g_lwm2m_lock = lwm2m_lock;
    g_db = db;

    g_httpd = MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION,
            port, NULL, NULL, &handler, NULL,
            MHD_OPTION_END);
    UASSERT(g_httpd);
    printf("microhttpd started on port %d\n", port);
}

void stop_httpd(void)
{
    MHD_stop_daemon(g_httpd);
}

static int handle_url(struct MHD_Connection *cn, const char *url, const uvector_t *parsed_url, const char *method);
static int get_devices_list(struct MHD_Connection *cn);
static int get_device_info(struct MHD_Connection *cn, const char *device_id);
static int get_sensors_list(struct MHD_Connection *cn, const char *device_id);
static int get_sensor_value(struct MHD_Connection *cn, const char *device_id, const char *sensor_id);
static int get_sensor_statistics(struct MHD_Connection *cn, const char *device_id, const char *sensor_id);

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

static int respond_404(struct MHD_Connection *con, const char *msg)
{
    int ret;
    struct MHD_Response *response;
    const char *NOT_FOUND_ERROR = "<html>"
                                  "<head><title>Not found</title></head>"
                                  "<body>%s</body>"
                                  "</html>\n";
    char *output = ustring_fmt(NOT_FOUND_ERROR, msg ? msg : "Go away, bitch.");
    response = MHD_create_response_from_buffer(strlen(output), (void *)output,
                                               MHD_RESPMEM_MUST_COPY);
    ret = MHD_queue_response(con, MHD_HTTP_NOT_FOUND, response);
    MHD_destroy_response(response);
    ufree(output);

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


    if ((0 != strcmp(method, "GET")) && (0 != strcmp(method, "POST")))
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

    printf("handling '%s %s'\n", method, url);

    if (0 != *upload_data_size)
    {
        return MHD_NO; /* upload data in a GET!? */
    }

    *ptr = NULL; /* clear context pointer */

    if (strlen(url) == 0)
    {
        return respond_404(cn, NULL);
    }
    if (url[0] != '/')
    {
        return respond_404(cn, NULL);
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
        return respond_404(cn, NULL);
    }

    int ret;
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
                else if (strcmp(t, "stat") == 0)
                {
                    return get_sensor_statistics(cn, device_id, sensor_id);
                }
            }
        }
    }
    else
    {
        url = (url[0] == '/' && url[1] == 0) ? "index.html" : url;
        char *path = ustring_fmt("www/%s", url);
        ugeneric_t g = ufile_read_to_memchunk(path);
        ufree(path);
        if (G_IS_ERROR(g))
        {
            ugeneric_error_print(g);
            respond_404(cn, G_AS_STR(g));
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

    return respond_404(cn, NULL);
}

static int get_devices_list(struct MHD_Connection *cn)
{
    int ret = 0;
    udict_t *td = udict_create();
    uvector_t *v = lwm2m_get_devices(g_lwm2m_ctx, g_lwm2m_lock);

    udict_put(td, G_CSTR("data"), G_VECTOR(v));

    char *str = udict_as_str(td);
    ret = respond_from_buffer(cn, str, strlen(str));
    udict_destroy(td);
    ufree(str);

    return ret;
}

static int get_sensors_list(struct MHD_Connection *cn, const char *device_id)
{

    printf("Getting sensors list for %s\n", device_id);

    lwm2m_client_t *c = find_device_by_id(device_id, g_lwm2m_ctx, g_lwm2m_lock);

    if (!c)
    {
        return respond_404(cn, NULL);
    }

    udict_t *d = udict_create();
    uvector_t *sensors = uvector_create();
    pthread_mutex_lock(g_lwm2m_lock);
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
    pthread_mutex_unlock(g_lwm2m_lock);

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
    char *response = NULL;
    size_t response_size = 0;
    int ret = 0;

    if (lwm2m_read_sensor(device_id, sensor_id, g_lwm2m_ctx, g_lwm2m_lock, &response, &response_size) != 0)
    {
        return respond_404(cn, NULL);
    }
    if (response_size == 0)
    {
        return respond_404(cn, NULL);
    }

    ugeneric_t g = ugeneric_parse(response);
    ufree(response);

    if (G_IS_ERROR(g))
    {
        ugeneric_error_print(g);
        ret = respond_from_buffer(cn, G_AS_STR(g), strlen(G_AS_STR(g)));
        ugeneric_error_destroy(g);
        goto invalid_json;
    }

    ugeneric_print(g, NULL);
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

    size_t sample_size;
    char *sample = ustring_fmt_sized("{\"data\": {\"type\": \"type\", \"value\": \"%d\"}}", &sample_size, G_AS_INT(v));
    ret = respond_from_buffer(cn, sample, sample_size);
    ufree(sample);
    ugeneric_destroy(g, NULL);
    return ret;

invalid_json:
    ugeneric_destroy(g, NULL);
    fprintf(stderr, "invalid response");
    return respond_404(cn, NULL);
}

static int get_sensor_statistics(struct MHD_Connection *cn, const char *device_id, const char *sensor_id)
{
    int ret = 0;
    uvector_t *samples = get_samples(g_db, device_id, sensor_id, 0, 1577836800);
    if (samples)
    {
        size_t stat_size = 0;
        char *t = uvector_as_str(samples);
        char *stat = ustring_fmt_sized("{\"data\": %s}}", &stat_size, t);
        ret = respond_from_buffer(cn, stat, stat_size);
        ufree(stat);
        ufree(t);
        uvector_destroy(samples);
    }
    else
    {
        ret = respond_404(cn, NULL);
    }

    return ret;
}
