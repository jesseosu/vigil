/*
 * main.c — vigild entry point
 *
 * Parses command-line arguments, loads configuration, initialises all
 * subsystems, and runs the main monitoring loop.
 *
 * Usage: vigild [-c config_path] [-f] [-v]
 *   -c  Path to TOML config file (default: /etc/vigil/vigil.toml)
 *   -f  Run in foreground (don't daemonise)
 *   -v  Verbose (debug-level logging)
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <getopt.h>

#include "daemon.h"
#include "config.h"
#include "log.h"
#include "ringbuf.h"
#include "collector.h"
#include "watchdog.h"
#include "ipc.h"

static void print_usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s [options]\n"
        "Options:\n"
        "  -c PATH   Config file path (default: %s)\n"
        "  -f        Run in foreground (don't daemonise)\n"
        "  -v        Verbose logging (debug level)\n"
        "  -h        Show this help\n",
        prog, VIGIL_DEFAULT_CONFIG_PATH);
}

int main(int argc, char *argv[])
{
    const char *config_path = VIGIL_DEFAULT_CONFIG_PATH;
    int foreground = 0;
    int verbose = 0;
    int opt;

    while ((opt = getopt(argc, argv, "c:fvh")) != -1) {
        switch (opt) {
        case 'c':
            config_path = optarg;
            break;
        case 'f':
            foreground = 1;
            break;
        case 'v':
            verbose = 1;
            break;
        case 'h':
            print_usage(argv[0]);
            return 0;
        default:
            print_usage(argv[0]);
            return 1;
        }
    }

    /* Load configuration */
    vigil_config_t cfg;
    if (config_load(&cfg, config_path) < 0) {
        fprintf(stderr, "vigild: failed to load config from %s\n", config_path);
        return 1;
    }

    /* Initialise logging */
    if (log_init(cfg.log_path) < 0) {
        fprintf(stderr, "vigild: failed to open log at %s\n", cfg.log_path);
        return 1;
    }

    if (verbose)
        log_set_level(LOG_DEBUG);

    /* Daemonise or run in foreground */
    if (!foreground) {
        if (daemon_init(cfg.pid_file) < 0) {
            log_event(LOG_ERROR, "daemon_init_failed", NULL);
            log_close();
            return 1;
        }
    } else {
        /* Foreground mode: just install signals and write PID file */
        if (daemon_install_signals() < 0) {
            fprintf(stderr, "vigild: failed to install signal handlers\n");
            log_close();
            return 1;
        }
        if (daemon_write_pidfile(cfg.pid_file) < 0) {
            fprintf(stderr, "vigild: another instance is running or cannot write PID file\n");
            log_close();
            return 1;
        }
    }

    log_event(LOG_INFO, "daemon_start",
              "i:pid", (int)getpid(),
              "config", config_path,
              NULL);

    /* Initialise ring buffer */
    ringbuf_t ring;
    if (ringbuf_init(&ring, (size_t)cfg.ring_capacity) < 0) {
        log_event(LOG_ERROR, "ringbuf_init_failed", NULL);
        daemon_cleanup(cfg.pid_file);
        log_close();
        return 1;
    }

    /* Initialise collectors */
    collectors_t collectors;
    if (collectors_init(&collectors) < 0) {
        log_event(LOG_ERROR, "collectors_init_failed", NULL);
        ringbuf_destroy(&ring);
        daemon_cleanup(cfg.pid_file);
        log_close();
        return 1;
    }

    /* Initialise watchdog */
    watchdog_init(&cfg);

    /* Initialise IPC server */
    ipc_server_t ipc;
    if (ipc_init(&ipc, cfg.socket_path) < 0) {
        log_event(LOG_WARN, "ipc_init_failed",
                  "path", cfg.socket_path, NULL);
        /* Non-fatal: daemon can run without IPC */
    }

    /* Record start time for uptime calculation */
    time_t start_time = time(NULL);

    /* ── Main loop ─────────────────────────────────────────── */
    struct timespec tick;
    tick.tv_sec  = cfg.tick_interval;
    tick.tv_nsec = 0;

    while (g_running) {
        /* Config reload on SIGHUP */
        if (g_reload) {
            if (config_reload(&cfg, config_path) == 0) {
                watchdog_init(&cfg);
                log_reopen();
                log_event(LOG_INFO, "config_reload", NULL);
            } else {
                log_event(LOG_ERROR, "config_reload_failed", NULL);
            }
            g_reload = 0;
        }

        /* Collect telemetry */
        collectors_tick(&collectors, &ring, &cfg);

        /* Check thresholds and fire actions */
        watchdog_check(&ring, &cfg);

        /* Handle IPC requests */
        ipc_poll(&ipc, &ring, &cfg, 100);

        /* Sleep until next tick (drift-free) */
        clock_nanosleep(CLOCK_MONOTONIC, 0, &tick, NULL);
    }

    /* ── Clean shutdown ────────────────────────────────────── */
    time_t uptime = time(NULL) - start_time;
    log_event(LOG_INFO, "daemon_stop",
              "i:uptime_seconds", (int)uptime,
              NULL);

    ipc_cleanup(&ipc);
    collectors_cleanup(&collectors);
    ringbuf_destroy(&ring);
    daemon_cleanup(cfg.pid_file);
    config_free(&cfg);
    log_close();

    return 0;
}
