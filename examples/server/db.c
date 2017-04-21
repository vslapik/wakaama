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
              "sensor_id INT PRIMARY KEY NOT NULL,"
              "name TEXT NOT NULL,"
              "unit TEXT);"
          "CREATE TABLE samples("
              "sensor_id INT,"
              "sample REAL,"
              "timestamp TIME,"
              "FOREIGN KEY (sensor_id) REFERENCES sensors(sensor_id));";

    rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "%s: SQL error: %s\n", __func__, err_msg);
        sqlite3_free(err_msg);
        abort();
    }
}

void insert_sensor(sqlite3 *db, int sensor_id, const char *name, const char *unit)
{
    const char *sql_fmt;
    char *sql;
    int rc;
    char *err_msg;

    sql_fmt = "INSERT INTO sensors VALUES(%d, '%s', '%s');";
    rc = asprintf(&sql, sql_fmt, sensor_id, name, unit);
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

void insert_sample(sqlite3 *db, int sensor_id, double sample, time_t ts)
{
    const char *sql_fmt;
    char *sql;
    int rc;
    char *err_msg;

    sql_fmt = "INSERT INTO samples VALUES(%d, %f, %ld);";
    rc = asprintf(&sql, sql_fmt, sensor_id, sample, ts);
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

char *get_samples(sqlite3 *db, int sensor_id, time_t from, time_t to)
{
    const char *sql_fmt;
    char *sql;
    sqlite3_stmt *stmt;
    int nrecs;
    int rc;
    char *err_msg;
    int i;
    int row = 0;

    sql_fmt = "SELECT * FROM samples where sensor_id = %d and timestamp >= %lld and timestamp <= %lld";
    rc = asprintf(&sql, sql_fmt, sensor_id, from, to);
    if (rc == -1)
    {
        abort();
    }
    puts(sql);

    sqlite3_prepare_v2(db, sql, strlen(sql) + 1, &stmt, NULL);
    free(sql);

    while (true)
    {
        int s;

        s = sqlite3_step (stmt);
        if (s == SQLITE_ROW) {
            int bytes;
            const unsigned char * text;
            bytes = sqlite3_column_bytes (stmt, 0);
            text  = sqlite3_column_text (stmt, 0);
            printf ("%d: %s\n", row, text);
            row++;
        }
        else if (s == SQLITE_DONE) {
            break;
        }
        else {
            fprintf (stderr, "Failed.\n");
            exit (1);
        }
    }
    return 0;
}
