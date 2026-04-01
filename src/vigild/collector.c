/*
 * collector.c — Collector dispatcher
 *
 * Initialises all collectors, dispatches per-tick sampling, and aggregates
 * results into a telemetry_sample_t pushed to the ring buffer.
 */

#define _POSIX_C_SOURCE 200809L

#include <string.h>
#include <time.h>

#include "collector.h"
#include "log.h"

int collectors_init(collectors_t *c)
{
    if (!c)
        return -1;

    memset(c, 0, sizeof(collectors_t));
    return 0;
}

int collectors_tick(collectors_t *c, ringbuf_t *rb, const vigil_config_t *cfg)
{
    telemetry_sample_t sample;
    memset(&sample, 0, sizeof(sample));

    /* Timestamp this sample */
    clock_gettime(CLOCK_REALTIME, &sample.timestamp);

    /* CPU */
    if (collector_cpu_sample(&c->cpu, &sample) < 0) {
        log_event(LOG_WARN, "collector_fail", "collector", "cpu", NULL);
    }

    /* Memory */
    if (collector_mem_sample(&c->mem, &sample) < 0) {
        log_event(LOG_WARN, "collector_fail", "collector", "memory", NULL);
    }

    /* Disk — use first configured device */
    if (cfg && cfg->n_disk_devices > 0) {
        if (collector_disk_sample(&c->disk, &sample, cfg->disk_devices[0]) < 0) {
            log_event(LOG_WARN, "collector_fail", "collector", "disk", NULL);
        }
    }

    /* Network — use first configured interface */
    if (cfg && cfg->n_net_interfaces > 0) {
        if (collector_net_sample(&c->net, &sample, cfg->net_interfaces[0]) < 0) {
            log_event(LOG_WARN, "collector_fail", "collector", "network", NULL);
        }
    }

    /* Push to ring buffer */
    ringbuf_push(rb, &sample);

    return 0;
}

void collectors_cleanup(collectors_t *c)
{
    if (c)
        memset(c, 0, sizeof(collectors_t));
}
