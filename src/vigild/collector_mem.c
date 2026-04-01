/*
 * collector_mem.c — Memory telemetry collector
 *
 * Reads /proc/meminfo for memory and swap usage.
 *
 * Key fields:
 *   MemTotal, MemAvailable, MemFree, Buffers, Cached
 *   SwapTotal, SwapFree
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>

#include "collector.h"

int collector_mem_sample(mem_state_t *state, telemetry_sample_t *out)
{
    FILE *fp;
    char line[256];
    unsigned long long mem_total = 0, mem_available = 0, mem_free = 0;
    unsigned long long buffers = 0, cached = 0;
    unsigned long long swap_total = 0, swap_free = 0;
    int fields_found = 0;

    (void)state;  /* mem collector is stateless */

    fp = fopen("/proc/meminfo", "r");
    if (!fp)
        return -1;

    while (fgets(line, sizeof(line), fp) && fields_found < 7) {
        unsigned long long val;

        if (sscanf(line, "MemTotal: %llu kB", &val) == 1) {
            mem_total = val;
            fields_found++;
        } else if (sscanf(line, "MemAvailable: %llu kB", &val) == 1) {
            mem_available = val;
            fields_found++;
        } else if (sscanf(line, "MemFree: %llu kB", &val) == 1) {
            mem_free = val;
            fields_found++;
        } else if (sscanf(line, "Buffers: %llu kB", &val) == 1) {
            buffers = val;
            fields_found++;
        } else if (sscanf(line, "Cached: %llu kB", &val) == 1) {
            cached = val;
            fields_found++;
        } else if (sscanf(line, "SwapTotal: %llu kB", &val) == 1) {
            swap_total = val;
            fields_found++;
        } else if (sscanf(line, "SwapFree: %llu kB", &val) == 1) {
            swap_free = val;
            fields_found++;
        }
    }

    fclose(fp);

    /* Compute memory usage percentage */
    if (mem_total > 0) {
        if (mem_available > 0) {
            /* Prefer MemAvailable (accounts for reclaimable caches) */
            out->mem_used_pct = 100.0 * (1.0 - (double)mem_available / (double)mem_total);
            out->mem_available_mb = (double)mem_available / 1024.0;
        } else {
            /* Fallback: total - free - buffers - cached */
            unsigned long long used = mem_total - mem_free - buffers - cached;
            out->mem_used_pct = 100.0 * (double)used / (double)mem_total;
            out->mem_available_mb = (double)(mem_free + buffers + cached) / 1024.0;
        }
    }

    /* Compute swap usage percentage */
    if (swap_total > 0) {
        unsigned long long swap_used = swap_total - swap_free;
        out->swap_used_pct = 100.0 * (double)swap_used / (double)swap_total;
    } else {
        out->swap_used_pct = 0.0;
    }

    return 0;
}
