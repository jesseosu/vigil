/*
 * cmd_health.c — vigilctl "health" command
 *
 * Sends a health request to vigild and displays pass/fail status
 * for monitored services, processes, and metric thresholds.
 *
 * Output format:
 *   vigil — health check
 *   ────────────────────
 *     ✓  nginx           running (pid 1823)
 *     ✗  redis           NOT FOUND
 *     ✓  CPU             23.4% (threshold: 90%)
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "client.h"

/* ANSI colours */
#define GREEN  "\033[32m"
#define RED    "\033[31m"
#define RESET  "\033[0m"

/* Check mark and cross mark (UTF-8) */
#define CHECK  "\xe2\x9c\x93"
#define CROSS  "\xe2\x9c\x97"

static double json_get_double(const char *json, const char *key)
{
    char search[128];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char *p = strstr(json, search);
    if (!p) return 0.0;
    p += strlen(search);
    while (*p == ' ') p++;
    return atof(p);
}

int cmd_health(const char *socket_path)
{
    int fd = client_connect(socket_path);
    if (fd < 0) {
        fprintf(stderr, "Error: cannot connect to vigild at %s\n", socket_path);
        fprintf(stderr, "Is the daemon running?\n");
        return 1;
    }

    const char *request = "{\"cmd\":\"health\"}";
    if (client_send(fd, request, strlen(request)) < 0) {
        fprintf(stderr, "Error: failed to send request\n");
        client_close(fd);
        return 1;
    }

    char *response = NULL;
    if (client_recv(fd, &response) < 0) {
        fprintf(stderr, "Error: failed to receive response\n");
        client_close(fd);
        return 1;
    }

    client_close(fd);

    /* Check for error */
    if (strstr(response, "\"ok\":false")) {
        fprintf(stderr, "Error: daemon returned error\n");
        free(response);
        return 1;
    }

    printf("vigil \xe2\x80\x94 health check\n");
    printf("\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
           "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
           "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
           "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
           "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\n");

    /* Extract and display metric health */
    double cpu_pct  = json_get_double(response, "cpu_pct");
    double mem_pct  = json_get_double(response, "mem_pct");
    double swap_pct = json_get_double(response, "swap_pct");

    /* CPU health (threshold typically 90%) */
    if (cpu_pct < 90.0)
        printf("  " GREEN CHECK RESET "  CPU             %.1f%%\n", cpu_pct);
    else
        printf("  " RED CROSS RESET "  CPU             %.1f%% " RED "(HIGH)" RESET "\n", cpu_pct);

    /* Memory health (threshold typically 95%) */
    if (mem_pct < 95.0)
        printf("  " GREEN CHECK RESET "  Memory          %.1f%%\n", mem_pct);
    else
        printf("  " RED CROSS RESET "  Memory          %.1f%% " RED "(HIGH)" RESET "\n", mem_pct);

    /* Swap health */
    if (swap_pct < 80.0)
        printf("  " GREEN CHECK RESET "  Swap            %.1f%%\n", swap_pct);
    else
        printf("  " RED CROSS RESET "  Swap            %.1f%% " RED "(HIGH)" RESET "\n", swap_pct);

    /* Parse and display individual rules from the response */
    const char *rules = strstr(response, "\"rules\":[");
    if (rules) {
        const char *p = rules + 9;
        while (*p) {
            const char *name_start = strstr(p, "\"name\":\"");
            if (!name_start)
                break;
            name_start += 8;
            const char *name_end = strchr(name_start, '"');
            if (!name_end)
                break;

            char name[64];
            size_t nlen = (size_t)(name_end - name_start);
            if (nlen >= sizeof(name))
                nlen = sizeof(name) - 1;
            memcpy(name, name_start, nlen);
            name[nlen] = '\0';

            /* Get threshold */
            const char *thresh_str = strstr(name_end, "\"threshold\":");
            double threshold = 0.0;
            if (thresh_str)
                threshold = atof(thresh_str + 12);

            /* Get breach count */
            const char *breach_str = strstr(name_end, "\"breaches\":");
            int breaches = 0;
            if (breach_str)
                breaches = atoi(breach_str + 11);

            if (breaches == 0)
                printf("  " GREEN CHECK RESET "  %-16s (threshold: %.0f%%)\n",
                       name, threshold);
            else
                printf("  " RED CROSS RESET "  %-16s " RED "breaching" RESET
                       " (threshold: %.0f%%, count: %d)\n",
                       name, threshold, breaches);

            /* Move to next rule */
            p = name_end + 1;
            const char *next_brace = strchr(p, '{');
            if (!next_brace)
                break;
            p = next_brace;
        }
    }

    free(response);
    return 0;
}
