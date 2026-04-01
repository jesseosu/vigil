/*
 * cmd_status.c — vigilctl "status" command
 *
 * Sends a status request to vigild and displays a formatted system snapshot.
 *
 * Output format:
 *   vigil — system status at 2026-04-15 10:30:00
 *   ─────────────────────────────────────────────
 *     CPU     23.4% user   5.1% sys   1.2% iowait
 *     Memory  67.2% (4.8 GB / 7.1 GB)   Swap 0.0%
 *     Disk    sda: 12.3 MB/s read  4.5 MB/s write
 *     Network eth0: 1.2 MB/s rx  0.3 MB/s tx
 *     Load    1.24  0.98  0.87
 *     Uptime  2h 14m
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "client.h"

/* Minimal JSON value extractor for doubles */
static double json_get_double(const char *json, const char *key)
{
    char search[128];
    snprintf(search, sizeof(search), "\"%s\":", key);

    const char *p = strstr(json, search);
    if (!p)
        return 0.0;

    p += strlen(search);
    while (*p == ' ')
        p++;

    return atof(p);
}

/* Extract a string value from JSON */
static int json_get_string(const char *json, const char *key,
                           char *out, size_t outlen)
{
    char search[128];
    snprintf(search, sizeof(search), "\"%s\":\"", key);

    const char *p = strstr(json, search);
    if (!p)
        return -1;

    p += strlen(search);
    const char *end = strchr(p, '"');
    if (!end)
        return -1;

    size_t len = (size_t)(end - p);
    if (len >= outlen)
        len = outlen - 1;

    memcpy(out, p, len);
    out[len] = '\0';
    return 0;
}

int cmd_status(const char *socket_path)
{
    int fd = client_connect(socket_path);
    if (fd < 0) {
        fprintf(stderr, "Error: cannot connect to vigild at %s\n", socket_path);
        fprintf(stderr, "Is the daemon running?\n");
        return 1;
    }

    const char *request = "{\"cmd\":\"status\"}";
    if (client_send(fd, request, strlen(request)) < 0) {
        fprintf(stderr, "Error: failed to send request\n");
        client_close(fd);
        return 1;
    }

    char *response = NULL;
    if (client_recv(fd, &response) < 0) {
        fprintf(stderr, "Error: failed to receive response\n");
        client_close(fd);
        return 1;
    }

    client_close(fd);

    /* Check for error response */
    if (strstr(response, "\"ok\":false")) {
        fprintf(stderr, "Error: daemon returned error\n");
        free(response);
        return 1;
    }

    /* Extract values */
    char timestamp[64];
    json_get_string(response, "timestamp", timestamp, sizeof(timestamp));

    double cpu_user   = json_get_double(response, "cpu_user_pct");
    double cpu_sys    = json_get_double(response, "cpu_system_pct");
    double cpu_iowait = json_get_double(response, "cpu_iowait_pct");
    double mem_pct    = json_get_double(response, "mem_used_pct");
    double mem_avail  = json_get_double(response, "mem_available_mb");
    double swap_pct   = json_get_double(response, "swap_used_pct");
    double disk_r     = json_get_double(response, "disk_read_bps");
    double disk_w     = json_get_double(response, "disk_write_bps");
    double net_rx     = json_get_double(response, "net_rx_bps");
    double net_tx     = json_get_double(response, "net_tx_bps");
    double load1      = json_get_double(response, "load_avg_1");
    double load5      = json_get_double(response, "load_avg_5");
    double load15     = json_get_double(response, "load_avg_15");

    free(response);

    /* Format timestamp for display */
    char display_time[32] = "now";
    if (timestamp[0] != '\0') {
        /* Convert ISO 8601 to readable format */
        snprintf(display_time, sizeof(display_time), "%.19s", timestamp);
        /* Replace 'T' with space */
        char *t = strchr(display_time, 'T');
        if (t)
            *t = ' ';
    }

    /* Print formatted output */
    printf("vigil \xe2\x80\x94 system status at %s\n", display_time);
    printf("\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
           "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
           "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
           "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
           "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
           "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
           "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
           "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
           "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
           "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\n");
    printf("  CPU     %.1f%% user   %.1f%% sys   %.1f%% iowait\n",
           cpu_user, cpu_sys, cpu_iowait);

    /* Calculate total memory from percentage and available */
    double mem_total_mb = (mem_pct > 0.01) ?
        mem_avail / (1.0 - mem_pct / 100.0) : mem_avail;
    double mem_used_mb = mem_total_mb - mem_avail;

    printf("  Memory  %.1f%% (%.1f GB / %.1f GB)   Swap %.1f%%\n",
           mem_pct, mem_used_mb / 1024.0, mem_total_mb / 1024.0, swap_pct);
    printf("  Disk    %.1f MB/s read  %.1f MB/s write\n",
           disk_r / (1024.0 * 1024.0), disk_w / (1024.0 * 1024.0));
    printf("  Network %.1f MB/s rx  %.1f MB/s tx\n",
           net_rx / (1024.0 * 1024.0), net_tx / (1024.0 * 1024.0));
    printf("  Load    %.2f  %.2f  %.2f\n", load1, load5, load15);

    return 0;
}
