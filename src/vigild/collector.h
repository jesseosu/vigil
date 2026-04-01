/*
 * collector.h — Collector dispatcher
 *
 * Manages all system telemetry collectors (CPU, memory, disk, network).
 * Each tick, all collectors are sampled and results aggregated into a
 * telemetry_sample_t pushed to the ring buffer.
 */

#ifndef VIGILD_COLLECTOR_H
#define VIGILD_COLLECTOR_H

#include "vigil/types.h"
#include "ringbuf.h"
#include "config.h"

/* CPU collector state — tracks previous jiffies for delta computation */
typedef struct {
    unsigned long long prev_user;
    unsigned long long prev_nice;
    unsigned long long prev_system;
    unsigned long long prev_idle;
    unsigned long long prev_iowait;
    unsigned long long prev_irq;
    unsigned long long prev_softirq;
    unsigned long long prev_steal;
    int initialized;
} cpu_state_t;

/* Memory collector state */
typedef struct {
    int initialized;
} mem_state_t;

/* Disk collector state — tracks previous counters for delta */
typedef struct {
    unsigned long long prev_reads;
    unsigned long long prev_read_sectors;
    unsigned long long prev_writes;
    unsigned long long prev_write_sectors;
    unsigned long long prev_io_ticks;
    int initialized;
} disk_state_t;

/* Network collector state — tracks previous counters for delta */
typedef struct {
    unsigned long long prev_rx_bytes;
    unsigned long long prev_tx_bytes;
    unsigned long long prev_rx_errors;
    unsigned long long prev_tx_errors;
    int initialized;
} net_state_t;

/* Aggregate collector state */
typedef struct {
    cpu_state_t  cpu;
    mem_state_t  mem;
    disk_state_t disk;
    net_state_t  net;
} collectors_t;

/* Initialize all collectors. Returns 0 on success. */
int collectors_init(collectors_t *c);

/* Sample all collectors, push aggregated result to ring buffer. */
int collectors_tick(collectors_t *c, ringbuf_t *rb, const vigil_config_t *cfg);

/* Clean up collector resources. */
void collectors_cleanup(collectors_t *c);

/* Individual collector sample functions (used by dispatcher and tests) */
int collector_cpu_sample(cpu_state_t *state, telemetry_sample_t *out);
int collector_mem_sample(mem_state_t *state, telemetry_sample_t *out);
int collector_disk_sample(disk_state_t *state, telemetry_sample_t *out,
                          const char *device);
int collector_net_sample(net_state_t *state, telemetry_sample_t *out,
                         const char *interface);

#endif /* VIGILD_COLLECTOR_H */
