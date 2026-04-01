/*
 * ringbuf.h — Fixed-size telemetry ring buffer
 *
 * Single-producer (main loop), read by watchdog and IPC handler
 * within the same thread context (no locking needed).
 */

#ifndef VIGILD_RINGBUF_H
#define VIGILD_RINGBUF_H

#include <stddef.h>
#include "vigil/types.h"

typedef struct {
    telemetry_sample_t *buf;
    size_t capacity;
    size_t head;    /* next write position */
    size_t count;   /* number of valid samples (≤ capacity) */
} ringbuf_t;

/* Allocate ring buffer with given capacity. Returns 0 on success, -1 on error. */
int ringbuf_init(ringbuf_t *rb, size_t capacity);

/* Push a sample into the ring buffer, overwriting oldest if full. */
void ringbuf_push(ringbuf_t *rb, const telemetry_sample_t *sample);

/* Copy the most recent sample into *out. Returns 0 on success, -1 if empty. */
int ringbuf_peek_latest(const ringbuf_t *rb, telemetry_sample_t *out);

/*
 * Copy the most recent N samples into out[] (newest first).
 * *count is set to the actual number copied (may be < n if buffer has fewer).
 * Returns 0 on success, -1 if empty.
 */
int ringbuf_peek_n(const ringbuf_t *rb, size_t n,
                   telemetry_sample_t *out, size_t *count);

/* Free ring buffer memory. */
void ringbuf_destroy(ringbuf_t *rb);

#endif /* VIGILD_RINGBUF_H */
