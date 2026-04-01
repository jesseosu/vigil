/*
 * config.c — Hand-rolled TOML parser for Vigil configuration
 *
 * Supports the subset of TOML needed by Vigil:
 *   - Key = value pairs (string, int, float, bool)
 *   - [section] headers
 *   - [[section.subsection]] array of tables
 *   - Inline arrays: ["a", "b", "c"]
 *   - # comments
 *
 * Intentionally hand-rolled to avoid external dependencies and demonstrate
 * structured format parsing skills.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "config.h"
#include "log.h"

/* Current parsing context */
typedef enum {
    SEC_NONE,
    SEC_DAEMON,
    SEC_COLLECTORS,
    SEC_WATCHDOG_RULE,
    SEC_WATCHDOG_PROCESS
} section_t;

/* Trim leading and trailing whitespace in place */
static char *trim(char *s)
{
    while (*s && isspace((unsigned char)*s))
        s++;
    if (*s == '\0')
        return s;

    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end))
        *end-- = '\0';

    return s;
}

/* Remove surrounding quotes from a string value */
static char *unquote(char *s)
{
    size_t len = strlen(s);
    if (len >= 2 && s[0] == '"' && s[len - 1] == '"') {
        s[len - 1] = '\0';
        return s + 1;
    }
    return s;
}

/* Parse an inline array of strings: ["a", "b", "c"] */
static int parse_string_array(const char *value, char out[][VIGIL_MAX_NAME],
                              int max_items)
{
    int count = 0;

    /* Find opening bracket */
    const char *p = strchr(value, '[');
    if (!p)
        return 0;
    p++;

    while (*p && *p != ']' && count < max_items) {
        /* Skip whitespace and commas */
        while (*p && (*p == ' ' || *p == ',' || *p == '\t'))
            p++;

        if (*p == '"') {
            p++; /* skip opening quote */
            const char *start = p;
            while (*p && *p != '"')
                p++;
            if (*p == '"') {
                size_t len = (size_t)(p - start);
                if (len >= VIGIL_MAX_NAME)
                    len = VIGIL_MAX_NAME - 1;
                memcpy(out[count], start, len);
                out[count][len] = '\0';
                count++;
                p++; /* skip closing quote */
            }
        } else if (*p == ']') {
            break;
        } else {
            p++;
        }
    }

    return count;
}

/* Parse a watchdog metric string into enum */
static watchdog_metric_t parse_metric(const char *s)
{
    if (strcmp(s, "cpu") == 0)  return METRIC_CPU;
    if (strcmp(s, "mem") == 0)  return METRIC_MEM;
    if (strcmp(s, "swap") == 0) return METRIC_SWAP;
    if (strcmp(s, "disk") == 0) return METRIC_DISK;
    if (strcmp(s, "load") == 0) return METRIC_LOAD;
    return METRIC_CPU;  /* default */
}

/* Parse a watchdog action string into enum */
static watchdog_action_t parse_action(const char *s)
{
    if (strcmp(s, "alert") == 0)   return ACTION_ALERT;
    if (strcmp(s, "restart") == 0) return ACTION_RESTART;
    if (strcmp(s, "script") == 0)  return ACTION_SCRIPT;
    return ACTION_ALERT;  /* default */
}

void config_defaults(vigil_config_t *cfg)
{
    memset(cfg, 0, sizeof(vigil_config_t));

    cfg->tick_interval = VIGIL_DEFAULT_TICK_INTERVAL;
    cfg->ring_capacity = VIGIL_DEFAULT_RING_CAPACITY;
    snprintf(cfg->pid_file,    sizeof(cfg->pid_file),    "%s", VIGIL_DEFAULT_PID_FILE);
    snprintf(cfg->socket_path, sizeof(cfg->socket_path), "%s", VIGIL_DEFAULT_SOCKET_PATH);
    snprintf(cfg->log_path,    sizeof(cfg->log_path),    "%s", VIGIL_DEFAULT_LOG_PATH);
}

int config_load(vigil_config_t *cfg, const char *path)
{
    FILE *fp;
    char line[1024];
    section_t section = SEC_NONE;

    if (!cfg || !path)
        return -1;

    config_defaults(cfg);

    fp = fopen(path, "r");
    if (!fp)
        return -1;

    while (fgets(line, sizeof(line), fp)) {
        /* Strip comments */
        char *hash = strchr(line, '#');
        if (hash)
            *hash = '\0';

        char *trimmed = trim(line);

        /* Skip empty lines */
        if (*trimmed == '\0')
            continue;

        /* Array of tables: [[watchdog.rules]] or [[watchdog.processes]] */
        if (trimmed[0] == '[' && trimmed[1] == '[') {
            char *end = strstr(trimmed, "]]");
            if (!end)
                continue;
            *end = '\0';
            char *sec_name = trim(trimmed + 2);

            if (strcmp(sec_name, "watchdog.rules") == 0) {
                section = SEC_WATCHDOG_RULE;
                if (cfg->n_rules < VIGIL_MAX_RULES)
                    cfg->n_rules++;
            } else if (strcmp(sec_name, "watchdog.processes") == 0) {
                section = SEC_WATCHDOG_PROCESS;
                if (cfg->n_processes < VIGIL_MAX_PROCESSES)
                    cfg->n_processes++;
            }
            continue;
        }

        /* Section header: [daemon], [collectors] */
        if (trimmed[0] == '[') {
            char *end = strchr(trimmed, ']');
            if (!end)
                continue;
            *end = '\0';
            char *sec_name = trim(trimmed + 1);

            if (strcmp(sec_name, "daemon") == 0)
                section = SEC_DAEMON;
            else if (strcmp(sec_name, "collectors") == 0)
                section = SEC_COLLECTORS;
            else
                section = SEC_NONE;
            continue;
        }

        /* Key = value */
        char *eq = strchr(trimmed, '=');
        if (!eq)
            continue;

        *eq = '\0';
        char *key   = trim(trimmed);
        char *value = trim(eq + 1);
        value = unquote(value);

        switch (section) {
        case SEC_DAEMON:
            if (strcmp(key, "tick_interval") == 0)
                cfg->tick_interval = atoi(value);
            else if (strcmp(key, "ring_capacity") == 0)
                cfg->ring_capacity = atoi(value);
            else if (strcmp(key, "pid_file") == 0)
                snprintf(cfg->pid_file, sizeof(cfg->pid_file), "%s", value);
            else if (strcmp(key, "socket_path") == 0)
                snprintf(cfg->socket_path, sizeof(cfg->socket_path), "%s", value);
            else if (strcmp(key, "log_path") == 0)
                snprintf(cfg->log_path, sizeof(cfg->log_path), "%s", value);
            break;

        case SEC_COLLECTORS:
            if (strcmp(key, "disk_devices") == 0) {
                cfg->n_disk_devices = parse_string_array(
                    eq + 1, cfg->disk_devices, VIGIL_MAX_DEVICES);
            } else if (strcmp(key, "net_interfaces") == 0) {
                cfg->n_net_interfaces = parse_string_array(
                    eq + 1, cfg->net_interfaces, VIGIL_MAX_INTERFACES);
            }
            break;

        case SEC_WATCHDOG_RULE: {
            int idx = cfg->n_rules - 1;
            if (idx < 0)
                break;
            watchdog_rule_t *r = &cfg->rules[idx];

            if (strcmp(key, "name") == 0)
                snprintf(r->name, sizeof(r->name), "%s", value);
            else if (strcmp(key, "metric") == 0)
                r->metric = parse_metric(value);
            else if (strcmp(key, "threshold") == 0)
                r->threshold = atof(value);
            else if (strcmp(key, "sustained_seconds") == 0)
                r->sustained_seconds = atoi(value);
            else if (strcmp(key, "action") == 0)
                r->action = parse_action(value);
            else if (strcmp(key, "action_target") == 0)
                snprintf(r->action_target, sizeof(r->action_target), "%s", value);
            else if (strcmp(key, "cooldown_seconds") == 0)
                r->cooldown_seconds = atoi(value);
            break;
        }

        case SEC_WATCHDOG_PROCESS: {
            int idx = cfg->n_processes - 1;
            if (idx < 0)
                break;
            process_monitor_t *p = &cfg->processes[idx];

            if (strcmp(key, "name") == 0)
                snprintf(p->name, sizeof(p->name), "%s", value);
            else if (strcmp(key, "action") == 0)
                p->action = parse_action(value);
            else if (strcmp(key, "action_target") == 0)
                snprintf(p->action_target, sizeof(p->action_target), "%s", value);
            else if (strcmp(key, "cooldown_seconds") == 0)
                p->cooldown_seconds = atoi(value);
            break;
        }

        case SEC_NONE:
            break;
        }
    }

    fclose(fp);

    /* Validate required fields */
    if (cfg->tick_interval <= 0)
        cfg->tick_interval = VIGIL_DEFAULT_TICK_INTERVAL;
    if (cfg->ring_capacity <= 0)
        cfg->ring_capacity = VIGIL_DEFAULT_RING_CAPACITY;

    return 0;
}

int config_reload(vigil_config_t *cfg, const char *path)
{
    config_free(cfg);
    return config_load(cfg, path);
}

void config_free(vigil_config_t *cfg)
{
    if (cfg)
        memset(cfg, 0, sizeof(vigil_config_t));
}
