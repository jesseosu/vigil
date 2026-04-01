/*
 * log.h — Structured JSON logging
 *
 * Writes JSON Lines to a log file. Each line is a self-contained JSON object.
 * Supports log rotation via log_reopen() (called on SIGHUP).
 */

#ifndef VIGILD_LOG_H
#define VIGILD_LOG_H

#include "vigil/types.h"

/* Initialize logger. Opens log file at path. Returns 0 on success. */
int log_init(const char *path);

/*
 * Write a structured log event.
 * fmt is a series of key-value pairs: "key1", val1, "key2", val2, ...
 * Terminated by a NULL key.
 *
 * Value types are inferred by prefix convention:
 *   "s:key" → string value (const char *)
 *   "d:key" → double value
 *   "i:key" → int value
 *   "key"   → string value (default)
 */
void log_event(log_level_t level, const char *event, ...);

/* Close and reopen the log file (for log rotation). */
void log_reopen(void);

/* Close the log file. */
void log_close(void);

/* Set minimum log level. Messages below this level are discarded. */
void log_set_level(log_level_t level);

#endif /* VIGILD_LOG_H */
