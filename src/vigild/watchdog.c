/*
 * watchdog.c — Threshold checking, breach counting, and process monitoring
 *
 * Per tick:
 *   1. Get latest sample from ring buffer
 *   2. For each rule: extract metric, check threshold, track breach count
 *   3. If breach_count >= sustained_seconds AND cooldown elapsed: fire action
 *   4. Scan /proc for missing monitored processes
 */

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <time.h>

#include "watchdog.h"
#include "action.h"
#include "log.h"

/* Extract the relevant metric value from a sample */
static double get_metric_value(const telemetry_sample_t *sample,
                               watchdog_metric_t metric)
{
    switch (metric) {
    case METRIC_CPU:
        return sample->cpu_user_pct + sample->cpu_system_pct;
    case METRIC_MEM:
        return sample->mem_used_pct;
    case METRIC_SWAP:
        return sample->swap_used_pct;
    case METRIC_DISK:
        /* Use write throughput as disk pressure indicator */
        return sample->disk_write_bps;
    case METRIC_LOAD:
        return sample->load_avg_1;
    default:
        return 0.0;
    }
}

static const char *metric_name(watchdog_metric_t metric)
{
    switch (metric) {
    case METRIC_CPU:  return "cpu";
    case METRIC_MEM:  return "mem";
    case METRIC_SWAP: return "swap";
    case METRIC_DISK: return "disk";
    case METRIC_LOAD: return "load";
    default:          return "unknown";
    }
}

int watchdog_init(vigil_config_t *cfg)
{
    if (!cfg)
        return -1;

    for (int i = 0; i < cfg->n_rules; i++) {
        cfg->rules[i].breach_count = 0;
        cfg->rules[i].last_action_time = 0;
    }

    for (int i = 0; i < cfg->n_processes; i++) {
        cfg->processes[i].last_action_time = 0;
    }

    return 0;
}

/* Check if a process with given name is running by scanning /proc */
static int process_is_running(const char *name)
{
    DIR *proc_dir;
    struct dirent *entry;

    proc_dir = opendir("/proc");
    if (!proc_dir)
        return 0;

    while ((entry = readdir(proc_dir)) != NULL) {
        /* Only check numeric directories (PIDs) */
        if (!isdigit((unsigned char)entry->d_name[0]))
            continue;

        char comm_path[300];
        snprintf(comm_path, sizeof(comm_path), "/proc/%s/comm", entry->d_name);

        FILE *fp = fopen(comm_path, "r");
        if (!fp)
            continue;

        char comm[256];
        if (fgets(comm, sizeof(comm), fp)) {
            /* Remove trailing newline */
            size_t len = strlen(comm);
            if (len > 0 && comm[len - 1] == '\n')
                comm[len - 1] = '\0';

            if (strcmp(comm, name) == 0) {
                fclose(fp);
                closedir(proc_dir);
                return 1;
            }
        }
        fclose(fp);
    }

    closedir(proc_dir);
    return 0;
}

int watchdog_check(ringbuf_t *rb, vigil_config_t *cfg)
{
    if (!rb || !cfg)
        return -1;

    telemetry_sample_t sample;
    if (ringbuf_peek_latest(rb, &sample) < 0)
        return 0;  /* No samples yet */

    time_t now = time(NULL);

    /* Check threshold rules */
    for (int i = 0; i < cfg->n_rules; i++) {
        watchdog_rule_t *r = &cfg->rules[i];
        double value = get_metric_value(&sample, r->metric);

        if (value > r->threshold) {
            r->breach_count++;
        } else {
            r->breach_count = 0;
            continue;
        }

        /* Check if sustained long enough and cooldown has passed */
        if (r->breach_count >= r->sustained_seconds &&
            (now - r->last_action_time) > r->cooldown_seconds) {

            log_event(LOG_WARN, "threshold_breach",
                      "rule", r->name,
                      "metric", metric_name(r->metric),
                      "d:value", value,
                      "d:threshold", r->threshold,
                      "i:sustained", r->breach_count,
                      NULL);

            /* Fire the configured action */
            switch (r->action) {
            case ACTION_ALERT:
                action_alert(r->name, value, r->threshold);
                break;
            case ACTION_RESTART:
                action_restart_service(r->action_target);
                break;
            case ACTION_SCRIPT:
                action_run_script(r->action_target, r->name);
                break;
            }

            log_event(LOG_INFO, "action_fired",
                      "rule", r->name,
                      "action", r->action == ACTION_ALERT ? "alert" :
                                r->action == ACTION_RESTART ? "restart" : "script",
                      NULL);

            r->last_action_time = now;
            r->breach_count = 0;
        }
    }

    /* Check process monitors */
    for (int i = 0; i < cfg->n_processes; i++) {
        process_monitor_t *p = &cfg->processes[i];

        if (!process_is_running(p->name)) {
            if ((now - p->last_action_time) > p->cooldown_seconds) {
                log_event(LOG_ERROR, "process_missing",
                          "process", p->name,
                          "action", p->action == ACTION_RESTART ? "restart" : "script",
                          "target", p->action_target,
                          NULL);

                switch (p->action) {
                case ACTION_ALERT:
                    action_alert(p->name, 0.0, 0.0);
                    break;
                case ACTION_RESTART:
                    action_restart_service(p->action_target);
                    break;
                case ACTION_SCRIPT:
                    action_run_script(p->action_target, p->name);
                    break;
                }

                p->last_action_time = now;
            }
        }
    }

    return 0;
}
