#ifndef __GLUE_H
#define __GLUE_H

#include <pthread.h>
#include <stdbool.h>
#include <liblwm2m.h>

typedef struct {
    pthread_cond_t data_wait;
    pthread_mutex_t data_lock;
    struct timespec timeout;
    bool data_available;
    void *data;
    size_t data_size;
} data_consumer_t;

data_consumer_t *data_consumer_create(struct timespec timeout);
void data_consumer_put_data(data_consumer_t *dc, void *data, size_t data_size);
void data_consumer_wait_for_data(data_consumer_t *dc);
void data_consumer_destroy(data_consumer_t *dc);


lwm2m_client_t *find_device_by_id(const char *device_id,
                                  lwm2m_context_t *lwm2m_ctx,
                                  pthread_mutex_t *lwm2m_lock);
int lwm2m_read_sensor(const char *device_id, const char *sensor_id,
                      lwm2m_context_t *lwm2m_ctx, pthread_mutex_t *lwm2m_lock,
                      char **data, size_t *data_size);

int lwm2m_write_sensor(const char *device_id, const char *sensor_id,
                       lwm2m_context_t *lwm2m_ctx, pthread_mutex_t *lwm2m_lock,
                       char **data, size_t *data_size);

uvector_t *lwm2m_get_devices(lwm2m_context_t *lwm2m_ctx, pthread_mutex_t *lwm2m_lock);

#endif
