/*
 * collector_cpu.c — CPU telemetry collector
 *
 * Reads /proc/stat for aggregate CPU jiffies and computes percentage
 * utilisation by delta from the previous tick. Also reads /proc/loadavg
 * for 1/5/15 minute load averages.
 *
 * /proc/stat first line format:
 *   cpu <user> <nice> <system> <idle> <iowait> <irq> <softirq> <steal> [guest] [guest_nice]
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>

#include "collector.h"

int collector_cpu_sample(cpu_state_t *state, telemetry_sample_t *out)
{
    FILE *fp;
    char line[512];
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;

    /* Parse /proc/stat */
    fp = fopen("/proc/stat", "r");
    if (!fp)
        return -1;

    if (!fgets(line, sizeof(line), fp)) {
        fclose(fp);
        return -1;
    }
    fclose(fp);

    /* Parse the aggregate CPU line */
    int matched = sscanf(line,
        "cpu  %llu %llu %llu %llu %llu %llu %llu %llu",
        &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal);

    if (matched < 8)
        return -1;

    if (state->initialized) {
        /* Compute deltas */
        unsigned long long d_user    = (user + nice) - (state->prev_user + state->prev_nice);
        unsigned long long d_system  = (system + irq + softirq) -
                                       (state->prev_system + state->prev_irq + state->prev_softirq);
        unsigned long long d_idle    = idle - state->prev_idle;
        unsigned long long d_iowait  = iowait - state->prev_iowait;
        unsigned long long d_steal   = steal - state->prev_steal;
        unsigned long long d_total   = d_user + d_system + d_idle + d_iowait + d_steal;

        if (d_total > 0) {
            out->cpu_user_pct   = 100.0 * (double)d_user   / (double)d_total;
            out->cpu_system_pct = 100.0 * (double)d_system  / (double)d_total;
            out->cpu_idle_pct   = 100.0 * (double)d_idle    / (double)d_total;
            out->cpu_iowait_pct = 100.0 * (double)d_iowait  / (double)d_total;
        } else {
            out->cpu_user_pct   = 0.0;
            out->cpu_system_pct = 0.0;
            out->cpu_idle_pct   = 100.0;
            out->cpu_iowait_pct = 0.0;
        }
    } else {
        /* First sample — no delta available yet */
        out->cpu_user_pct   = 0.0;
        out->cpu_system_pct = 0.0;
        out->cpu_idle_pct   = 100.0;
        out->cpu_iowait_pct = 0.0;
        state->initialized  = 1;
    }

    /* Store current values for next delta */
    state->prev_user    = user;
    state->prev_nice    = nice;
    state->prev_system  = system;
    state->prev_idle    = idle;
    state->prev_iowait  = iowait;
    state->prev_irq     = irq;
    state->prev_softirq = softirq;
    state->prev_steal   = steal;

    /* Parse /proc/loadavg */
    fp = fopen("/proc/loadavg", "r");
    if (fp) {
        double l1, l5, l15;
        if (fscanf(fp, "%lf %lf %lf", &l1, &l5, &l15) == 3) {
            out->load_avg_1  = l1;
            out->load_avg_5  = l5;
            out->load_avg_15 = l15;
        }
        fclose(fp);
    }

    return 0;
}
