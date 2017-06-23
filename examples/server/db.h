#ifndef __DB_H
#define __DB_H

#include <sqlite3.h>
#include <ugeneric.h>

void create_tables(sqlite3 *db);
void save_sensor(sqlite3 *db, const char *device_id, const char *sensor_id, const char *name, const char *unit);
void insert_sample(sqlite3 *db, const char *device_id, const char *sensor_id, const char *sample, time_t ts);
uvector_t *get_samples(sqlite3 *db, const char *device_id, const char *sensor_id, time_t from, time_t to);

#endif
