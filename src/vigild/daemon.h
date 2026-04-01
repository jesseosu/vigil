/*
 * daemon.h — Daemonisation, PID file, signal handling
 *
 * Implements the traditional Unix double-fork daemon lifecycle.
 */

#ifndef VIGILD_DAEMON_H
#define VIGILD_DAEMON_H

#include <signal.h>

/* Global flags set by signal handlers (async-signal-safe) */
extern volatile sig_atomic_t g_running;
extern volatile sig_atomic_t g_reload;

/*
 * Daemonise the process:
 *   1. fork() — parent exits
 *   2. setsid() — new session
 *   3. Second fork() — prevent reacquiring terminal
 *   4. chdir("/")
 *   5. Close and redirect stdin/stdout/stderr to /dev/null
 *   6. Write PID file with flock()
 *   7. Install signal handlers
 *
 * pid_file: path for PID file (e.g. /var/run/vigil.pid)
 * Returns 0 on success, -1 on error.
 */
int daemon_init(const char *pid_file);

/*
 * Install signal handlers only (for foreground mode).
 * Returns 0 on success, -1 on error.
 */
int daemon_install_signals(void);

/*
 * Write PID file with exclusive flock.
 * Returns 0 on success, -1 if another instance holds the lock.
 */
int daemon_write_pidfile(const char *pid_file);

/*
 * Clean up: remove PID file and release lock.
 */
void daemon_cleanup(const char *pid_file);

#endif /* VIGILD_DAEMON_H */
