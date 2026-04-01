/*
 * main.c — vigilctl CLI entry point
 *
 * Usage:
 *   vigilctl status              One-shot system snapshot
 *   vigilctl top [--interval N]  Live-updating display
 *   vigilctl health              Pass/fail health check
 *   vigilctl logs [--lines N]    Tail structured log
 *   vigilctl --help              Show usage
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

/* Command handlers (defined in cmd_*.c) */
extern int cmd_status(const char *socket_path);
extern int cmd_top(const char *socket_path, int interval);
extern int cmd_health(const char *socket_path);
extern int cmd_logs(const char *log_path, int lines, int follow);

#define DEFAULT_SOCKET_PATH "/var/run/vigil.sock"
#define DEFAULT_LOG_PATH    "/var/log/vigil/vigil.jsonl"

static void print_usage(void)
{
    printf(
        "vigilctl — Vigil system monitor CLI\n"
        "\n"
        "Usage:\n"
        "  vigilctl status                 Show current system status\n"
        "  vigilctl top [--interval N]     Live-updating display (default: 1s)\n"
        "  vigilctl health                 Health check (pass/fail)\n"
        "  vigilctl logs [--lines N]       Tail structured log (default: 20)\n"
        "  vigilctl --help                 Show this help\n"
        "\n"
        "Options:\n"
        "  -s, --socket PATH   Socket path (default: %s)\n"
        "  -l, --log PATH      Log file path (default: %s)\n"
        "  -i, --interval N    Refresh interval for 'top' (seconds)\n"
        "  -n, --lines N       Number of log lines to show\n"
        "  -f, --follow        Follow log file for new lines\n"
        "  -h, --help          Show this help\n",
        DEFAULT_SOCKET_PATH, DEFAULT_LOG_PATH);
}

int main(int argc, char *argv[])
{
    const char *socket_path = DEFAULT_SOCKET_PATH;
    const char *log_path = DEFAULT_LOG_PATH;
    int interval = 1;
    int lines = 20;
    int follow = 0;

    if (argc < 2) {
        print_usage();
        return 1;
    }

    /* Check if first arg is a command or an option */
    const char *command = NULL;
    int cmd_arg_idx = 1;

    if (argv[1][0] != '-') {
        command = argv[1];
        cmd_arg_idx = 2;
    }

    /* Parse options after the command */
    static struct option long_options[] = {
        {"socket",   required_argument, 0, 's'},
        {"log",      required_argument, 0, 'l'},
        {"interval", required_argument, 0, 'i'},
        {"lines",    required_argument, 0, 'n'},
        {"follow",   no_argument,       0, 'f'},
        {"help",     no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    optind = cmd_arg_idx;
    int opt;
    while ((opt = getopt_long(argc, argv, "s:l:i:n:fh", long_options, NULL)) != -1) {
        switch (opt) {
        case 's':
            socket_path = optarg;
            break;
        case 'l':
            log_path = optarg;
            break;
        case 'i':
            interval = atoi(optarg);
            break;
        case 'n':
            lines = atoi(optarg);
            break;
        case 'f':
            follow = 1;
            break;
        case 'h':
            print_usage();
            return 0;
        default:
            print_usage();
            return 1;
        }
    }

    if (!command) {
        print_usage();
        return 1;
    }

    /* Dispatch to command handler */
    if (strcmp(command, "status") == 0)
        return cmd_status(socket_path);
    if (strcmp(command, "top") == 0)
        return cmd_top(socket_path, interval);
    if (strcmp(command, "health") == 0)
        return cmd_health(socket_path);
    if (strcmp(command, "logs") == 0)
        return cmd_logs(log_path, lines, follow);

    fprintf(stderr, "Unknown command: %s\n\n", command);
    print_usage();
    return 1;
}
