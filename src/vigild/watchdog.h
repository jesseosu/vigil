/*
 * watchdog.h — Threshold checking and anomaly detection
 *
 * Each tick, the watchdog checks the latest telemetry sample against
 * configured rules. Rules have sustained-breach counting and cooldown
 * to prevent action spam.
 */

#ifndef VIGILD_WATCHDOG_H
#define VIGILD_WATCHDOG_H

#include "vigil/types.h"
#include "ringbuf.h"
#include "config.h"

/* Initialize watchdog state (reset breach counts and timers). */
int watchdog_init(vigil_config_t *cfg);

/*
 * Check the latest sample against all rules and process monitors.
 * Fires actions when thresholds are breached for the sustained duration.
 */
int watchdog_check(ringbuf_t *rb, vigil_config_t *cfg);

#endif /* VIGILD_WATCHDOG_H */
