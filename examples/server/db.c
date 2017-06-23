#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "db.h"

void create_tables(sqlite3 *db)
{
    char *sql;
    int rc;
    char *err_msg;

    sql = "DROP TABLE IF EXISTS sensors;"
          "DROP TABLE IF EXISTS samples;"
          "CREATE TABLE sensors("
              "device_id TEXT NOT NULL,"
              "sensor_id TEXT NOT NULL,"
              "name TEXT NOT NULL,"
              "unit TEXT,"
              "UNIQUE(device_id, sensor_id)"
              "PRIMARY KEY (device_id, sensor_id));"
          "CREATE TABLE samples("
              "device_id TEXT NOT NULL,"
              "sensor_id TEXT NOT NULL,"
              "sample TEXT NOT NULL,"
              "timestamp TIME,"
              "FOREIGN KEY (device_id, sensor_id) REFERENCES sensors(device_id, sensor_id));";

    rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "%s: SQL error: %s\n", __func__, err_msg);
        sqlite3_free(err_msg);
        abort();
    }
}

void save_sensor(sqlite3 *db, const char *device_id, const char *sensor_id, const char *name, const char *unit)
{
    const char *sql_fmt;
    char *sql;
    int rc;
    char *err_msg;

    sql_fmt = "INSERT OR IGNORE INTO sensors VALUES('%s', '%s', '%s', '%s');";
    rc = asprintf(&sql, sql_fmt, device_id, sensor_id, name, unit);
    if (rc == -1)
    {
        abort();
    }
    puts(sql);

    rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    free(sql);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "%s: SQL error: %s\n", __func__, err_msg);
        sqlite3_free(err_msg);
        abort();
    }
}

void insert_sample(sqlite3 *db, const char *device_id, const char *sensor_id, const char *sample, time_t ts)
{
    const char *sql_fmt;
    char *sql;
    int rc;
    char *err_msg;

    sql_fmt = "INSERT INTO samples VALUES('%s', '%s', '%s', %ld);";
    rc = asprintf(&sql, sql_fmt, device_id, sensor_id, sample, ts);
    if (rc == -1)
    {
        abort();
    }
    puts(sql);

    rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    free(sql);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "%s: SQL error: %s\n", __func__, err_msg);
        sqlite3_free(err_msg);
        abort();
    }
}

uvector_t *get_samples(sqlite3 *db, const char *device_id, const char *sensor_id, time_t from, time_t to)
{
    const char *sql_fmt;
    char *sql;
    sqlite3_stmt *stmt;
    int nrecs;
    int rc;
    char *err_msg;
    int i;
    int row = 0;

    sql_fmt = "SELECT timestamp,sample FROM samples where device_id = '%s' and sensor_id = '%s' and timestamp >= %lld and timestamp <= %lld";
    rc = asprintf(&sql, sql_fmt, device_id, sensor_id, from, to);
    if (rc == -1)
    {
        abort();
    }
    puts(sql);

    sqlite3_prepare_v2(db, sql, strlen(sql) + 1, &stmt, NULL);
    free(sql);

    uvector_t *samples = NULL;
    while (true)
    {
        int s = sqlite3_step(stmt);
        if (s == SQLITE_ROW)
        {
            if (!samples)
            {
                samples = uvector_create();
            }

            const unsigned char *ts = sqlite3_column_text (stmt, 0);
            const unsigned char *sample = sqlite3_column_text (stmt, 1);
            udict_t *t = udict_create();
            udict_put(t, G_STR(ustring_dup(ts)), G_STR(ustring_dup(sample)));
            uvector_append(samples, G_DICT(t));
        }
        else if (s == SQLITE_DONE)
        {
            break;
        }
        else
        {
            fprintf (stderr, "Failed.\n");
            exit(1);
        }
    }

    sqlite3_finalize(stmt);
    return samples;
}
