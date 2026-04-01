/*
 * action.h — Remediation actions
 *
 * Actions that the watchdog engine can trigger:
 *   - Alert: log a structured JSON event
 *   - Restart: restart a systemd service via systemctl
 *   - Script: execute a custom script
 */

#ifndef VIGILD_ACTION_H
#define VIGILD_ACTION_H

/* Log an alert event for a threshold breach. Returns 0 on success. */
int action_alert(const char *rule_name, double value, double threshold);

/*
 * Restart a systemd service via fork() + exec("systemctl", "restart", ...).
 * Waits for child with a 30-second timeout.
 * Returns 0 on success, -1 on error.
 */
int action_restart_service(const char *service_name);

/*
 * Execute a custom remediation script via fork() + exec().
 * The rule name is passed as argv[1].
 * Returns 0 on success, -1 on error.
 */
int action_run_script(const char *script_path, const char *rule_name);

#endif /* VIGILD_ACTION_H */
