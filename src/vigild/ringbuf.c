/*
 * ringbuf.c — Fixed-size telemetry ring buffer
 *
 * O(1) push, O(1) peek_latest, O(n) peek_n.
 * Fixed memory footprint — single malloc at init, no allocations during operation.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <string.h>
#include "ringbuf.h"

int ringbuf_init(ringbuf_t *rb, size_t capacity)
{
    if (!rb || capacity == 0)
        return -1;

    rb->buf = calloc(capacity, sizeof(telemetry_sample_t));
    if (!rb->buf)
        return -1;

    rb->capacity = capacity;
    rb->head = 0;
    rb->count = 0;
    return 0;
}

void ringbuf_push(ringbuf_t *rb, const telemetry_sample_t *sample)
{
    if (!rb || !rb->buf || !sample)
        return;

    memcpy(&rb->buf[rb->head], sample, sizeof(telemetry_sample_t));
    rb->head = (rb->head + 1) % rb->capacity;

    if (rb->count < rb->capacity)
        rb->count++;
}

int ringbuf_peek_latest(const ringbuf_t *rb, telemetry_sample_t *out)
{
    if (!rb || !rb->buf || !out || rb->count == 0)
        return -1;

    /* head points to next write position, so latest is at head - 1 */
    size_t idx = (rb->head == 0) ? rb->capacity - 1 : rb->head - 1;
    memcpy(out, &rb->buf[idx], sizeof(telemetry_sample_t));
    return 0;
}

int ringbuf_peek_n(const ringbuf_t *rb, size_t n,
                   telemetry_sample_t *out, size_t *count)
{
    if (!rb || !rb->buf || !out || !count || rb->count == 0)
        return -1;

    size_t actual = (n < rb->count) ? n : rb->count;
    *count = actual;

    /* Walk backwards from the most recent entry */
    size_t idx = (rb->head == 0) ? rb->capacity - 1 : rb->head - 1;
    for (size_t i = 0; i < actual; i++) {
        memcpy(&out[i], &rb->buf[idx], sizeof(telemetry_sample_t));
        idx = (idx == 0) ? rb->capacity - 1 : idx - 1;
    }

    return 0;
}

void ringbuf_destroy(ringbuf_t *rb)
{
    if (!rb)
        return;

    free(rb->buf);
    rb->buf = NULL;
    rb->capacity = 0;
    rb->head = 0;
    rb->count = 0;
}
