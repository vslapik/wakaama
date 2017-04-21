#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "rest.h"

struct MHD_Daemon *g_httpd;

static int handler(void *cls,
        struct MHD_Connection *connection,
        const char *url,
        const char *method,
        const char *version,
        const char *upload_data,
        size_t *upload_data_size,
        void **ptr);

void start_httpd(void)
{
    g_httpd = MHD_start_daemon(MHD_USE_THREAD_PER_CONNECTION,
            8888, NULL, NULL, &handler, NULL,
            MHD_OPTION_END);
    assert(g_httpd);
}

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


    const char *buf = "fuck you bitch\n";
    response = MHD_create_response_from_buffer(strlen(buf),
            (void*) buf, MHD_RESPMEM_PERSISTENT);

    ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);

    return ret;
}

void stop_httpd(void)
{
    MHD_stop_daemon(g_httpd);
}
