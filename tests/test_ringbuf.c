/*
 * test_ringbuf.c — Unit tests for the telemetry ring buffer
 *
 * Tests: init, push, peek_latest, peek_n, wraparound, destroy.
 * Uses a simple assert-based framework — exits on first failure.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "vigild/ringbuf.h"

#define TEST(name) do { printf("  [TEST] %s... ", #name); name(); printf("PASS\n"); } while(0)

static void test_init_destroy(void)
{
    ringbuf_t rb;
    assert(ringbuf_init(&rb, 10) == 0);
    assert(rb.capacity == 10);
    assert(rb.head == 0);
    assert(rb.count == 0);
    assert(rb.buf != NULL);
    ringbuf_destroy(&rb);
    assert(rb.buf == NULL);
}

static void test_init_invalid(void)
{
    ringbuf_t rb;
    assert(ringbuf_init(NULL, 10) == -1);
    assert(ringbuf_init(&rb, 0) == -1);
}

static void test_push_and_peek_latest(void)
{
    ringbuf_t rb;
    ringbuf_init(&rb, 5);

    telemetry_sample_t sample;
    memset(&sample, 0, sizeof(sample));
    sample.cpu_user_pct = 42.0;

    ringbuf_push(&rb, &sample);
    assert(rb.count == 1);
    assert(rb.head == 1);

    telemetry_sample_t out;
    assert(ringbuf_peek_latest(&rb, &out) == 0);
    assert(fabs(out.cpu_user_pct - 42.0) < 0.001);

    ringbuf_destroy(&rb);
}

static void test_peek_empty(void)
{
    ringbuf_t rb;
    ringbuf_init(&rb, 5);

    telemetry_sample_t out;
    assert(ringbuf_peek_latest(&rb, &out) == -1);

    size_t count;
    assert(ringbuf_peek_n(&rb, 3, &out, &count) == -1);

    ringbuf_destroy(&rb);
}

static void test_peek_n(void)
{
    ringbuf_t rb;
    ringbuf_init(&rb, 10);

    /* Push 5 samples with increasing CPU values */
    for (int i = 0; i < 5; i++) {
        telemetry_sample_t s;
        memset(&s, 0, sizeof(s));
        s.cpu_user_pct = (double)(i + 1) * 10.0;
        ringbuf_push(&rb, &s);
    }

    assert(rb.count == 5);

    /* Peek last 3 — should get 50, 40, 30 (newest first) */
    telemetry_sample_t out[3];
    size_t count;
    assert(ringbuf_peek_n(&rb, 3, out, &count) == 0);
    assert(count == 3);
    assert(fabs(out[0].cpu_user_pct - 50.0) < 0.001);
    assert(fabs(out[1].cpu_user_pct - 40.0) < 0.001);
    assert(fabs(out[2].cpu_user_pct - 30.0) < 0.001);

    /* Peek more than available */
    telemetry_sample_t out2[10];
    assert(ringbuf_peek_n(&rb, 10, out2, &count) == 0);
    assert(count == 5);

    ringbuf_destroy(&rb);
}

static void test_wraparound(void)
{
    ringbuf_t rb;
    ringbuf_init(&rb, 3);

    /* Push 5 samples into a buffer of capacity 3 */
    for (int i = 0; i < 5; i++) {
        telemetry_sample_t s;
        memset(&s, 0, sizeof(s));
        s.cpu_user_pct = (double)(i + 1) * 10.0;
        ringbuf_push(&rb, &s);
    }

    /* Count should be capped at capacity */
    assert(rb.count == 3);

    /* Latest should be 50 (the 5th push) */
    telemetry_sample_t out;
    assert(ringbuf_peek_latest(&rb, &out) == 0);
    assert(fabs(out.cpu_user_pct - 50.0) < 0.001);

    /* Peek 3 should get 50, 40, 30 */
    telemetry_sample_t out3[3];
    size_t count;
    assert(ringbuf_peek_n(&rb, 3, out3, &count) == 0);
    assert(count == 3);
    assert(fabs(out3[0].cpu_user_pct - 50.0) < 0.001);
    assert(fabs(out3[1].cpu_user_pct - 40.0) < 0.001);
    assert(fabs(out3[2].cpu_user_pct - 30.0) < 0.001);

    ringbuf_destroy(&rb);
}

static void test_head_wraps_correctly(void)
{
    ringbuf_t rb;
    ringbuf_init(&rb, 4);

    /* Push exactly capacity items */
    for (int i = 0; i < 4; i++) {
        telemetry_sample_t s;
        memset(&s, 0, sizeof(s));
        s.mem_used_pct = (double)i;
        ringbuf_push(&rb, &s);
    }

    assert(rb.head == 0);  /* Should wrap back to 0 */
    assert(rb.count == 4);

    /* Push one more — overwrites oldest */
    telemetry_sample_t s;
    memset(&s, 0, sizeof(s));
    s.mem_used_pct = 99.0;
    ringbuf_push(&rb, &s);

    assert(rb.head == 1);
    assert(rb.count == 4);

    telemetry_sample_t out;
    assert(ringbuf_peek_latest(&rb, &out) == 0);
    assert(fabs(out.mem_used_pct - 99.0) < 0.001);

    ringbuf_destroy(&rb);
}

int main(void)
{
    printf("=== Ring Buffer Tests ===\n");

    TEST(test_init_destroy);
    TEST(test_init_invalid);
    TEST(test_push_and_peek_latest);
    TEST(test_peek_empty);
    TEST(test_peek_n);
    TEST(test_wraparound);
    TEST(test_head_wraps_correctly);

    printf("\nAll ring buffer tests passed.\n");
    return 0;
}
