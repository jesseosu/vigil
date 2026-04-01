/*
 * cmd_top.c — vigilctl "top" command
 *
 * Live-updating terminal display, refreshing every N seconds.
 * Uses ANSI escape codes for colours:
 *   Green  (< 70%)
 *   Yellow (70-90%)
 *   Red    (> 90%)
 *
 * Exits on 'q' keypress. Sets terminal to raw mode for non-blocking input.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <time.h>
#include <poll.h>

#include "client.h"

/* ANSI colour codes */
#define ANSI_GREEN   "\033[32m"
#define ANSI_YELLOW  "\033[33m"
#define ANSI_RED     "\033[31m"
#define ANSI_RESET   "\033[0m"
#define ANSI_BOLD    "\033[1m"
#define CLEAR_SCREEN "\033[H\033[J"

static const char *colour_for_pct(double pct)
{
    if (pct > 90.0) return ANSI_RED;
    if (pct > 70.0) return ANSI_YELLOW;
    return ANSI_GREEN;
}

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

static struct termios g_orig_termios;
static int g_raw_mode = 0;

static void disable_raw_mode(void)
{
    if (g_raw_mode) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig_termios);
        g_raw_mode = 0;
    }
}

static void enable_raw_mode(void)
{
    tcgetattr(STDIN_FILENO, &g_orig_termios);
    struct termios raw = g_orig_termios;
    raw.c_lflag &= ~((unsigned)ECHO | (unsigned)ICANON);
    raw.c_cc[VMIN]  = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    g_raw_mode = 1;
}

int cmd_top(const char *socket_path, int interval)
{
    if (interval <= 0)
        interval = 1;

    enable_raw_mode();
    atexit(disable_raw_mode);

    printf(CLEAR_SCREEN);
    printf("vigil top — press 'q' to quit\n\n");

    while (1) {
        /* Check for 'q' keypress */
        struct pollfd pfd;
        pfd.fd = STDIN_FILENO;
        pfd.events = POLLIN;
        if (poll(&pfd, 1, 0) > 0) {
            char c;
            if (read(STDIN_FILENO, &c, 1) == 1 && (c == 'q' || c == 'Q'))
                break;
        }

        /* Connect and get status */
        int fd = client_connect(socket_path);
        if (fd < 0) {
            printf(CLEAR_SCREEN);
            printf(ANSI_RED "vigil top — cannot connect to daemon" ANSI_RESET "\n");
            printf("Retrying in %d seconds... (press 'q' to quit)\n", interval);
            sleep((unsigned)interval);
            continue;
        }

        const char *request = "{\"cmd\":\"status\"}";
        client_send(fd, request, strlen(request));

        char *response = NULL;
        int ret = client_recv(fd, &response);
        client_close(fd);

        if (ret < 0 || !response) {
            sleep((unsigned)interval);
            continue;
        }

        /* Parse values */
        double cpu_user   = json_get_double(response, "cpu_user_pct");
        double cpu_sys    = json_get_double(response, "cpu_system_pct");
        double cpu_iowait = json_get_double(response, "cpu_iowait_pct");
        double cpu_total  = cpu_user + cpu_sys;
        double mem_pct    = json_get_double(response, "mem_used_pct");
        double swap_pct   = json_get_double(response, "swap_used_pct");
        double mem_avail  = json_get_double(response, "mem_available_mb");
        double disk_r     = json_get_double(response, "disk_read_bps");
        double disk_w     = json_get_double(response, "disk_write_bps");
        double net_rx     = json_get_double(response, "net_rx_bps");
        double net_tx     = json_get_double(response, "net_tx_bps");
        double load1      = json_get_double(response, "load_avg_1");
        double load5      = json_get_double(response, "load_avg_5");
        double load15     = json_get_double(response, "load_avg_15");

        free(response);

        /* Get current time */
        time_t now = time(NULL);
        struct tm tm;
        localtime_r(&now, &tm);
        char timebuf[32];
        strftime(timebuf, sizeof(timebuf), "%H:%M:%S", &tm);

        /* Draw screen */
        printf(CLEAR_SCREEN);
        printf(ANSI_BOLD "vigil top" ANSI_RESET " — %s  (interval: %ds, press 'q' to quit)\n\n",
               timebuf, interval);

        printf("  CPU     %s%.1f%%\033[0m user   %.1f%% sys   %.1f%% iowait\n",
               colour_for_pct(cpu_total), cpu_user, cpu_sys, cpu_iowait);

        double mem_total_mb = (mem_pct > 0.01) ?
            mem_avail / (1.0 - mem_pct / 100.0) : mem_avail;

        printf("  Memory  %s%.1f%%\033[0m (%.1f GB / %.1f GB)   Swap %s%.1f%%\033[0m\n",
               colour_for_pct(mem_pct), mem_pct,
               (mem_total_mb - mem_avail) / 1024.0, mem_total_mb / 1024.0,
               colour_for_pct(swap_pct), swap_pct);
        printf("  Disk    %.1f MB/s read  %.1f MB/s write\n",
               disk_r / (1024.0 * 1024.0), disk_w / (1024.0 * 1024.0));
        printf("  Network %.1f MB/s rx  %.1f MB/s tx\n",
               net_rx / (1024.0 * 1024.0), net_tx / (1024.0 * 1024.0));
        printf("  Load    %.2f  %.2f  %.2f\n", load1, load5, load15);

        fflush(stdout);
        sleep((unsigned)interval);
    }

    disable_raw_mode();
    printf("\n");
    return 0;
}
