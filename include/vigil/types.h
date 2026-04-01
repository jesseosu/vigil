/*
 * vigil/types.h — Shared type definitions for Vigil
 *
 * Defines the telemetry sample structure used by collectors, ring buffer,
 * watchdog engine, and IPC protocol.
 */

#ifndef VIGIL_TYPES_H
#define VIGIL_TYPES_H

#include <time.h>
#include <stddef.h>
#include <stdint.h>

/* Maximum lengths for configuration strings */
#define VIGIL_MAX_NAME        64
#define VIGIL_MAX_PATH        256
#define VIGIL_MAX_DEVICES     16
#define VIGIL_MAX_INTERFACES  16
#define VIGIL_MAX_RULES       32
#define VIGIL_MAX_PROCESSES   32
#define VIGIL_MAX_CLIENTS     16

/* Default configuration values */
#define VIGIL_DEFAULT_TICK_INTERVAL   1
#define VIGIL_DEFAULT_RING_CAPACITY   3600
#define VIGIL_DEFAULT_PID_FILE        "/var/run/vigil.pid"
#define VIGIL_DEFAULT_SOCKET_PATH     "/var/run/vigil.sock"
#define VIGIL_DEFAULT_LOG_PATH        "/var/log/vigil/vigil.jsonl"
#define VIGIL_DEFAULT_CONFIG_PATH     "/etc/vigil/vigil.toml"

/*
 * Telemetry sample — one snapshot of system state per tick.
 * Stored in the ring buffer.
 */
typedef struct {
    struct timespec timestamp;
    double cpu_user_pct;
    double cpu_system_pct;
    double cpu_iowait_pct;
    double cpu_idle_pct;
    double mem_used_pct;
    double mem_available_mb;
    double swap_used_pct;
    double disk_read_bps;
    double disk_write_bps;
    double net_rx_bps;
    double net_tx_bps;
    double load_avg_1;
    double load_avg_5;
    double load_avg_15;
} telemetry_sample_t;

/* Log levels */
typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO  = 1,
    LOG_WARN  = 2,
    LOG_ERROR = 3
} log_level_t;

/* Watchdog metric types */
typedef enum {
    METRIC_CPU = 0,
    METRIC_MEM,
    METRIC_SWAP,
    METRIC_DISK,
    METRIC_LOAD
} watchdog_metric_t;

/* Remediation action types */
typedef enum {
    ACTION_ALERT = 0,
    ACTION_RESTART,
    ACTION_SCRIPT
} watchdog_action_t;

/* Watchdog rule — loaded from config */
typedef struct {
    char name[VIGIL_MAX_NAME];
    watchdog_metric_t metric;
    double threshold;
    int sustained_seconds;
    watchdog_action_t action;
    char action_target[VIGIL_MAX_PATH];
    int cooldown_seconds;
    /* Runtime state (not from config) */
    int breach_count;
    time_t last_action_time;
} watchdog_rule_t;

/* Process monitor entry — loaded from config */
typedef struct {
    char name[VIGIL_MAX_NAME];
    watchdog_action_t action;
    char action_target[VIGIL_MAX_PATH];
    int cooldown_seconds;
    /* Runtime state */
    time_t last_action_time;
} process_monitor_t;

#endif /* VIGIL_TYPES_H */
