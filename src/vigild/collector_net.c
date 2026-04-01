/*
 * collector_net.c — Network telemetry collector
 *
 * Reads /proc/net/dev for per-interface byte and packet counters.
 *
 * /proc/net/dev format (after 2 header lines):
 *   iface: rx_bytes rx_packets rx_errors rx_drops rx_fifo rx_frame rx_compressed rx_multicast
 *          tx_bytes tx_packets tx_errors tx_drops tx_fifo tx_colls tx_carrier tx_compressed
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>

#include "collector.h"

int collector_net_sample(net_state_t *state, telemetry_sample_t *out,
                         const char *interface)
{
    FILE *fp;
    char line[512];
    int found = 0;
    int line_num = 0;

    if (!interface || interface[0] == '\0')
        return 0;  /* No interface configured, skip silently */

    fp = fopen("/proc/net/dev", "r");
    if (!fp)
        return -1;

    while (fgets(line, sizeof(line), fp)) {
        line_num++;
        /* Skip 2 header lines */
        if (line_num <= 2)
            continue;

        /* Find the interface name (before the colon) */
        char *colon = strchr(line, ':');
        if (!colon)
            continue;

        /* Extract interface name, trimming leading whitespace */
        char iface[64];
        size_t len = (size_t)(colon - line);
        if (len >= sizeof(iface))
            continue;

        /* Trim leading spaces */
        const char *start = line;
        while (*start == ' ' || *start == '\t')
            start++;
        len = (size_t)(colon - start);
        if (len >= sizeof(iface))
            continue;

        memcpy(iface, start, len);
        iface[len] = '\0';

        if (strcmp(iface, interface) != 0)
            continue;

        found = 1;

        /* Parse counters after the colon */
        unsigned long long rx_bytes, rx_packets, rx_errors, rx_drops;
        unsigned long long rx_fifo, rx_frame, rx_compressed, rx_multicast;
        unsigned long long tx_bytes, tx_packets, tx_errors, tx_drops;

        int matched = sscanf(colon + 1,
            " %llu %llu %llu %llu %llu %llu %llu %llu"
            " %llu %llu %llu %llu",
            &rx_bytes, &rx_packets, &rx_errors, &rx_drops,
            &rx_fifo, &rx_frame, &rx_compressed, &rx_multicast,
            &tx_bytes, &tx_packets, &tx_errors, &tx_drops);

        if (matched < 12)
            break;

        if (state->initialized) {
            out->net_rx_bps = (double)(rx_bytes - state->prev_rx_bytes);
            out->net_tx_bps = (double)(tx_bytes - state->prev_tx_bytes);
        } else {
            out->net_rx_bps = 0.0;
            out->net_tx_bps = 0.0;
            state->initialized = 1;
        }

        state->prev_rx_bytes  = rx_bytes;
        state->prev_tx_bytes  = tx_bytes;
        state->prev_rx_errors = rx_errors;
        state->prev_tx_errors = tx_errors;
        break;
    }

    fclose(fp);

    if (!found) {
        out->net_rx_bps = 0.0;
        out->net_tx_bps = 0.0;
    }

    return 0;
}
