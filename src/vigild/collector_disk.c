/*
 * collector_disk.c — Disk I/O telemetry collector
 *
 * Reads /proc/diskstats for per-device I/O counters.
 *
 * /proc/diskstats format (selected fields):
 *   major minor name reads_completed reads_merged sectors_read time_reading
 *   writes_completed writes_merged sectors_written time_writing
 *   io_in_progress io_time weighted_io_time
 *
 * Fields of interest (0-indexed after name):
 *   0: reads completed
 *   2: sectors read
 *   4: writes completed
 *   6: sectors written
 *   9: io time (ms)
 *
 * Sector size: 512 bytes
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>

#include "collector.h"

#define SECTOR_SIZE 512

int collector_disk_sample(disk_state_t *state, telemetry_sample_t *out,
                          const char *device)
{
    FILE *fp;
    char line[512];
    int found = 0;

    if (!device || device[0] == '\0')
        return 0;  /* No device configured, skip silently */

    fp = fopen("/proc/diskstats", "r");
    if (!fp)
        return -1;

    while (fgets(line, sizeof(line), fp)) {
        unsigned int major, minor;
        char name[64];
        unsigned long long reads, reads_merged, read_sectors, read_time;
        unsigned long long writes, writes_merged, write_sectors, write_time;
        unsigned long long io_pending, io_ticks;

        int matched = sscanf(line,
            " %u %u %63s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
            &major, &minor, name,
            &reads, &reads_merged, &read_sectors, &read_time,
            &writes, &writes_merged, &write_sectors, &write_time,
            &io_pending, &io_ticks);

        if (matched < 13)
            continue;

        if (strcmp(name, device) != 0)
            continue;

        found = 1;

        if (state->initialized) {
            unsigned long long d_read_sectors  = read_sectors  - state->prev_read_sectors;
            unsigned long long d_write_sectors = write_sectors - state->prev_write_sectors;

            /* Convert sectors/tick to bytes/second */
            out->disk_read_bps  = (double)(d_read_sectors  * SECTOR_SIZE);
            out->disk_write_bps = (double)(d_write_sectors * SECTOR_SIZE);
        } else {
            out->disk_read_bps  = 0.0;
            out->disk_write_bps = 0.0;
            state->initialized  = 1;
        }

        state->prev_reads        = reads;
        state->prev_read_sectors = read_sectors;
        state->prev_writes       = writes;
        state->prev_write_sectors = write_sectors;
        state->prev_io_ticks     = io_ticks;
        break;
    }

    fclose(fp);

    if (!found) {
        out->disk_read_bps  = 0.0;
        out->disk_write_bps = 0.0;
    }

    return 0;
}
