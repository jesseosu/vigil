/*
 * config.h — Configuration management
 *
 * Parses a TOML configuration file into a vigil_config_t struct.
 * Supports [sections], [[array.tables]], key = value, inline arrays.
 */

#ifndef VIGILD_CONFIG_H
#define VIGILD_CONFIG_H

#include "vigil/types.h"

/* Configuration structure */
typedef struct {
    /* [daemon] */
    int tick_interval;
    int ring_capacity;
    char pid_file[VIGIL_MAX_PATH];
    char socket_path[VIGIL_MAX_PATH];
    char log_path[VIGIL_MAX_PATH];

    /* [collectors] */
    char disk_devices[VIGIL_MAX_DEVICES][VIGIL_MAX_NAME];
    int  n_disk_devices;
    char net_interfaces[VIGIL_MAX_INTERFACES][VIGIL_MAX_NAME];
    int  n_net_interfaces;

    /* [[watchdog.rules]] */
    watchdog_rule_t rules[VIGIL_MAX_RULES];
    int n_rules;

    /* [[watchdog.processes]] */
    process_monitor_t processes[VIGIL_MAX_PROCESSES];
    int n_processes;
} vigil_config_t;

/* Load configuration from a TOML file. Returns 0 on success, -1 on error. */
int config_load(vigil_config_t *cfg, const char *path);

/* Reload configuration (free old, load new). Returns 0 on success. */
int config_reload(vigil_config_t *cfg, const char *path);

/* Set default values in config struct. */
void config_defaults(vigil_config_t *cfg);

/* Free any dynamically allocated config resources. */
void config_free(vigil_config_t *cfg);

#endif /* VIGILD_CONFIG_H */
