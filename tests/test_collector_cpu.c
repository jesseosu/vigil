/*
 * test_collector_cpu.c — Unit tests for the CPU collector
 *
 * Tests against live /proc/stat to verify parsing and sane output ranges.
 */

#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "vigild/collector.h"

#define TEST(name) do { printf("  [TEST] %s... ", #name); name(); printf("PASS\n"); } while(0)

static void test_cpu_first_sample(void)
{
    cpu_state_t state;
    memset(&state, 0, sizeof(state));

    telemetry_sample_t sample;
    memset(&sample, 0, sizeof(sample));

    /* First sample should succeed but return zeroes (no delta yet) */
    int ret = collector_cpu_sample(&state, &sample);
    assert(ret == 0);
    assert(state.initialized == 1);

    /* CPU idle should be 100% on first sample (no delta) */
    assert(sample.cpu_idle_pct >= 0.0);
}

static void test_cpu_second_sample(void)
{
    cpu_state_t state;
    memset(&state, 0, sizeof(state));

    telemetry_sample_t s1, s2;
    memset(&s1, 0, sizeof(s1));
    memset(&s2, 0, sizeof(s2));

    /* First sample (initialises state) */
    assert(collector_cpu_sample(&state, &s1) == 0);

    /* Wait a bit for CPU counters to change */
    usleep(100000);  /* 100ms */

    /* Second sample should have real percentages */
    assert(collector_cpu_sample(&state, &s2) == 0);

    /* Percentages must be in valid range */
    assert(s2.cpu_user_pct >= 0.0 && s2.cpu_user_pct <= 100.0);
    assert(s2.cpu_system_pct >= 0.0 && s2.cpu_system_pct <= 100.0);
    assert(s2.cpu_idle_pct >= 0.0 && s2.cpu_idle_pct <= 100.0);
    assert(s2.cpu_iowait_pct >= 0.0 && s2.cpu_iowait_pct <= 100.0);

    /* Total should approximately sum to 100% */
    double total = s2.cpu_user_pct + s2.cpu_system_pct +
                   s2.cpu_idle_pct + s2.cpu_iowait_pct;
    assert(total > 95.0 && total < 105.0);  /* Allow small rounding */
}

static void test_cpu_load_avg(void)
{
    cpu_state_t state;
    memset(&state, 0, sizeof(state));

    telemetry_sample_t sample;
    memset(&sample, 0, sizeof(sample));

    assert(collector_cpu_sample(&state, &sample) == 0);

    /* Load averages should be non-negative */
    assert(sample.load_avg_1 >= 0.0);
    assert(sample.load_avg_5 >= 0.0);
    assert(sample.load_avg_15 >= 0.0);
}

int main(void)
{
    printf("=== CPU Collector Tests ===\n");

    TEST(test_cpu_first_sample);
    TEST(test_cpu_second_sample);
    TEST(test_cpu_load_avg);

    printf("\nAll CPU collector tests passed.\n");
    return 0;
}
