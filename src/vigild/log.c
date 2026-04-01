/*
 * log.c — Structured JSON logging (JSON Lines format)
 *
 * Each log line: {"ts":"2026-04-15T10:30:00.123Z","level":"info","event":"...","key":"val",...}
 * Uses fprintf + fflush for crash safety (unbuffered writes).
 * Timestamps from CLOCK_REALTIME with millisecond precision.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>

#include "log.h"

static FILE *g_log_fp;
static char g_log_path[256];
static log_level_t g_min_level = LOG_INFO;

static const char *level_str(log_level_t level)
{
    switch (level) {
    case LOG_DEBUG: return "debug";
    case LOG_INFO:  return "info";
    case LOG_WARN:  return "warn";
    case LOG_ERROR: return "error";
    default:        return "unknown";
    }
}

static void format_timestamp(char *buf, size_t len)
{
    struct timespec ts;
    struct tm tm;

    clock_gettime(CLOCK_REALTIME, &ts);
    gmtime_r(&ts.tv_sec, &tm);

    int n = (int)strftime(buf, len, "%Y-%m-%dT%H:%M:%S", &tm);
    snprintf(buf + n, len - (size_t)n, ".%03ldZ", ts.tv_nsec / 1000000);
}

int log_init(const char *path)
{
    if (!path)
        return -1;

    snprintf(g_log_path, sizeof(g_log_path), "%s", path);

    g_log_fp = fopen(g_log_path, "a");
    if (!g_log_fp)
        return -1;

    return 0;
}

void log_event(log_level_t level, const char *event, ...)
{
    if (level < g_min_level)
        return;

    FILE *fp = g_log_fp ? g_log_fp : stderr;
    char ts[64];
    format_timestamp(ts, sizeof(ts));

    fprintf(fp, "{\"ts\":\"%s\",\"level\":\"%s\",\"event\":\"%s\"",
            ts, level_str(level), event);

    va_list ap;
    va_start(ap, event);

    const char *key;
    while ((key = va_arg(ap, const char *)) != NULL) {
        if (strncmp(key, "d:", 2) == 0) {
            /* Double value */
            double val = va_arg(ap, double);
            fprintf(fp, ",\"%s\":%.1f", key + 2, val);
        } else if (strncmp(key, "i:", 2) == 0) {
            /* Integer value */
            int val = va_arg(ap, int);
            fprintf(fp, ",\"%s\":%d", key + 2, val);
        } else {
            /* String value — with or without "s:" prefix */
            const char *actual_key = key;
            if (strncmp(key, "s:", 2) == 0)
                actual_key = key + 2;
            const char *val = va_arg(ap, const char *);
            if (val)
                fprintf(fp, ",\"%s\":\"%s\"", actual_key, val);
            else
                fprintf(fp, ",\"%s\":null", actual_key);
        }
    }

    va_end(ap);

    fprintf(fp, "}\n");
    fflush(fp);
}

void log_reopen(void)
{
    if (g_log_fp) {
        fclose(g_log_fp);
        g_log_fp = NULL;
    }

    if (g_log_path[0] != '\0') {
        g_log_fp = fopen(g_log_path, "a");
    }
}

void log_close(void)
{
    if (g_log_fp) {
        fclose(g_log_fp);
        g_log_fp = NULL;
    }
}

void log_set_level(log_level_t level)
{
    g_min_level = level;
}
