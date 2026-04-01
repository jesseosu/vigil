/*
 * daemon.c — Daemonisation, PID file management, signal handling
 *
 * Double-fork daemonisation sequence:
 *   fork → setsid → fork → chdir("/") → close fds → redirect to /dev/null
 *
 * PID file uses flock(LOCK_EX | LOCK_NB) to prevent duplicate instances.
 * Signal handlers set volatile sig_atomic_t flags for the main loop.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/file.h>

#include "daemon.h"
#include "log.h"

volatile sig_atomic_t g_running = 1;
volatile sig_atomic_t g_reload  = 0;

static int g_pidfile_fd = -1;

static void handle_term(int sig)
{
    (void)sig;
    g_running = 0;
}

static void handle_hup(int sig)
{
    (void)sig;
    g_reload = 1;
}

int daemon_install_signals(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));

    /* SIGTERM / SIGINT → clean shutdown */
    sa.sa_handler = handle_term;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGTERM, &sa, NULL) < 0)
        return -1;
    if (sigaction(SIGINT, &sa, NULL) < 0)
        return -1;

    /* SIGHUP → reload config */
    sa.sa_handler = handle_hup;
    if (sigaction(SIGHUP, &sa, NULL) < 0)
        return -1;

    /* SIGPIPE → ignore (broken socket writes) */
    sa.sa_handler = SIG_IGN;
    if (sigaction(SIGPIPE, &sa, NULL) < 0)
        return -1;

    return 0;
}

int daemon_write_pidfile(const char *pid_file)
{
    if (!pid_file)
        return -1;

    g_pidfile_fd = open(pid_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (g_pidfile_fd < 0)
        return -1;

    /* Exclusive non-blocking lock — fails if another instance holds it */
    if (flock(g_pidfile_fd, LOCK_EX | LOCK_NB) < 0) {
        close(g_pidfile_fd);
        g_pidfile_fd = -1;
        return -1;
    }

    char buf[32];
    int n = snprintf(buf, sizeof(buf), "%d\n", (int)getpid());
    if (write(g_pidfile_fd, buf, (size_t)n) != n) {
        close(g_pidfile_fd);
        g_pidfile_fd = -1;
        return -1;
    }

    /* Keep fd open to maintain the lock */
    return 0;
}

int daemon_init(const char *pid_file)
{
    pid_t pid;

    /* First fork — parent exits */
    pid = fork();
    if (pid < 0)
        return -1;
    if (pid > 0)
        _exit(0);

    /* New session — detach from terminal */
    if (setsid() < 0)
        return -1;

    /* Second fork — prevent reacquiring a terminal */
    pid = fork();
    if (pid < 0)
        return -1;
    if (pid > 0)
        _exit(0);

    /* Don't hold mount points */
    if (chdir("/") < 0)
        return -1;

    /* Set file creation mask */
    umask(0027);

    /* Close standard file descriptors */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    /* Redirect fd 0/1/2 to /dev/null */
    int devnull = open("/dev/null", O_RDWR);
    if (devnull >= 0) {
        dup2(devnull, STDIN_FILENO);
        dup2(devnull, STDOUT_FILENO);
        dup2(devnull, STDERR_FILENO);
        if (devnull > STDERR_FILENO)
            close(devnull);
    }

    /* Write PID file */
    if (daemon_write_pidfile(pid_file) < 0)
        return -1;

    /* Install signal handlers */
    if (daemon_install_signals() < 0)
        return -1;

    return 0;
}

void daemon_cleanup(const char *pid_file)
{
    if (g_pidfile_fd >= 0) {
        flock(g_pidfile_fd, LOCK_UN);
        close(g_pidfile_fd);
        g_pidfile_fd = -1;
    }

    if (pid_file)
        unlink(pid_file);
}
