/*
 * action.c — Remediation actions
 *
 * Implements alert logging, service restart via systemctl, and custom
 * script execution. Child processes are forked with a 30-second timeout
 * to prevent hanging.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#include "action.h"
#include "log.h"

#define ACTION_TIMEOUT_SECONDS 30

/*
 * Fork and exec a command with a timeout.
 * Returns the child's exit status, or -1 on error.
 */
static int fork_exec_timeout(char *const argv[], int timeout_secs)
{
    pid_t pid = fork();

    if (pid < 0) {
        log_event(LOG_ERROR, "fork_failed", NULL);
        return -1;
    }

    if (pid == 0) {
        /* Child process */
        execvp(argv[0], argv);
        _exit(127);  /* exec failed */
    }

    /* Parent: wait for child with timeout */
    int status;
    int elapsed = 0;

    while (elapsed < timeout_secs) {
        pid_t ret = waitpid(pid, &status, WNOHANG);
        if (ret < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (ret > 0) {
            if (WIFEXITED(status))
                return WEXITSTATUS(status);
            return -1;
        }

        /* Child still running — sleep 1 second and check again */
        sleep(1);
        elapsed++;
    }

    /* Timeout — kill the child */
    log_event(LOG_WARN, "action_timeout",
              "i:timeout", timeout_secs, NULL);
    kill(pid, SIGKILL);
    waitpid(pid, &status, 0);
    return -1;
}

int action_alert(const char *rule_name, double value, double threshold)
{
    log_event(LOG_WARN, "alert",
              "rule", rule_name,
              "d:value", value,
              "d:threshold", threshold,
              NULL);
    return 0;
}

int action_restart_service(const char *service_name)
{
    if (!service_name || service_name[0] == '\0') {
        log_event(LOG_ERROR, "action_error",
                  "reason", "empty service name", NULL);
        return -1;
    }

    log_event(LOG_INFO, "restart_service",
              "service", service_name, NULL);

    char *argv[] = {
        "systemctl", "restart", (char *)service_name, NULL
    };

    int ret = fork_exec_timeout(argv, ACTION_TIMEOUT_SECONDS);

    if (ret == 0) {
        log_event(LOG_INFO, "restart_success",
                  "service", service_name, NULL);
    } else {
        log_event(LOG_ERROR, "restart_failed",
                  "service", service_name,
                  "i:exit_code", ret,
                  NULL);
    }

    return ret;
}

int action_run_script(const char *script_path, const char *rule_name)
{
    if (!script_path || script_path[0] == '\0') {
        log_event(LOG_ERROR, "action_error",
                  "reason", "empty script path", NULL);
        return -1;
    }

    log_event(LOG_INFO, "run_script",
              "script", script_path,
              "rule", rule_name ? rule_name : "",
              NULL);

    char *argv[] = {
        (char *)script_path,
        (char *)(rule_name ? rule_name : ""),
        NULL
    };

    int ret = fork_exec_timeout(argv, ACTION_TIMEOUT_SECONDS);

    if (ret == 0) {
        log_event(LOG_INFO, "script_success",
                  "script", script_path, NULL);
    } else {
        log_event(LOG_ERROR, "script_failed",
                  "script", script_path,
                  "i:exit_code", ret,
                  NULL);
    }

    return ret;
}
