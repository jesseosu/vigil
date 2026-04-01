/*
 * cmd_logs.c — vigilctl "logs" command
 *
 * Reads the structured JSON log file directly (doesn't go through daemon).
 * Parses JSON lines and pretty-prints with colours by log level.
 *
 * Options:
 *   --lines N   Show last N lines (default 20)
 *   --follow    Watch for new lines via inotify
 */

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <poll.h>

/* ANSI colours */
#define GREY    "\033[90m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define RED     "\033[31m"
#define RESET   "\033[0m"
#define BOLD    "\033[1m"

static const char *colour_for_level(const char *level)
{
    if (strcmp(level, "debug") == 0) return GREY;
    if (strcmp(level, "info") == 0)  return GREEN;
    if (strcmp(level, "warn") == 0)  return YELLOW;
    if (strcmp(level, "error") == 0) return RED;
    return RESET;
}

/* Extract a JSON string value (simple parser) */
static int extract_json_string(const char *json, const char *key,
                               char *out, size_t outlen)
{
    char search[128];
    snprintf(search, sizeof(search), "\"%s\":\"", key);
    const char *p = strstr(json, search);
    if (!p)
        return -1;

    p += strlen(search);
    const char *end = strchr(p, '"');
    if (!end)
        return -1;

    size_t len = (size_t)(end - p);
    if (len >= outlen)
        len = outlen - 1;

    memcpy(out, p, len);
    out[len] = '\0';
    return 0;
}

/* Pretty-print a single JSON log line */
static void print_log_line(const char *line)
{
    char ts[64] = "", level[16] = "", event[64] = "";

    extract_json_string(line, "ts", ts, sizeof(ts));
    extract_json_string(line, "level", level, sizeof(level));
    extract_json_string(line, "event", event, sizeof(event));

    const char *colour = colour_for_level(level);

    /* Format: timestamp [LEVEL] event: rest-of-json */
    printf("%s%-24s%s %s[%-5s]%s %s%s%s",
           GREY, ts, RESET,
           colour, level, RESET,
           BOLD, event, RESET);

    /* Print remaining key-value pairs (simplified) */
    /* Find content after "event":"..." */
    char search[128];
    snprintf(search, sizeof(search), "\"event\":\"%s\"", event);
    const char *after = strstr(line, search);
    if (after) {
        after += strlen(search);
        if (*after == ',') {
            /* Print the rest minus the closing brace */
            const char *end = strrchr(after, '}');
            if (end) {
                size_t len = (size_t)(end - after);
                char *rest = malloc(len + 1);
                if (rest) {
                    memcpy(rest, after + 1, len - 1);
                    rest[len - 1] = '\0';
                    if (rest[0] != '\0')
                        printf("  %s", rest);
                    free(rest);
                }
            }
        }
    }

    printf("\n");
}

/* Read last N lines from the log file */
static int tail_log(const char *path, int lines)
{
    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "Error: cannot open log file %s\n", path);
        return 1;
    }

    /* Read all lines into a circular buffer */
    char **line_buf = calloc((size_t)lines, sizeof(char *));
    if (!line_buf) {
        fclose(fp);
        return 1;
    }

    char buf[4096];
    int total = 0;

    while (fgets(buf, sizeof(buf), fp)) {
        int idx = total % lines;
        free(line_buf[idx]);
        line_buf[idx] = strdup(buf);
        total++;
    }

    fclose(fp);

    /* Print the last N lines in order */
    int count = (total < lines) ? total : lines;
    int start = (total < lines) ? 0 : total % lines;

    for (int i = 0; i < count; i++) {
        int idx = (start + i) % lines;
        if (line_buf[idx]) {
            /* Remove trailing newline */
            size_t len = strlen(line_buf[idx]);
            if (len > 0 && line_buf[idx][len - 1] == '\n')
                line_buf[idx][len - 1] = '\0';

            print_log_line(line_buf[idx]);
        }
    }

    for (int i = 0; i < lines; i++)
        free(line_buf[i]);
    free(line_buf);

    return 0;
}

/* Follow the log file for new lines using inotify */
static int follow_log(const char *path)
{
    int ifd = inotify_init();
    if (ifd < 0) {
        fprintf(stderr, "Error: inotify_init failed\n");
        return 1;
    }

    int wd = inotify_add_watch(ifd, path, IN_MODIFY);
    if (wd < 0) {
        fprintf(stderr, "Error: cannot watch %s\n", path);
        close(ifd);
        return 1;
    }

    FILE *fp = fopen(path, "r");
    if (!fp) {
        close(ifd);
        return 1;
    }

    /* Seek to end */
    fseek(fp, 0, SEEK_END);

    printf("Following %s (Ctrl+C to stop)...\n", path);

    char line[4096];
    struct pollfd pfd;
    pfd.fd = ifd;
    pfd.events = POLLIN;

    while (1) {
        /* Check for new data */
        while (fgets(line, sizeof(line), fp)) {
            size_t len = strlen(line);
            if (len > 0 && line[len - 1] == '\n')
                line[len - 1] = '\0';
            print_log_line(line);
        }

        clearerr(fp);

        /* Wait for inotify event */
        if (poll(&pfd, 1, 1000) > 0) {
            /* Drain inotify events */
            char ibuf[4096];
            ssize_t ignore = read(ifd, ibuf, sizeof(ibuf));
            (void)ignore;
        }
    }

    fclose(fp);
    close(ifd);
    return 0;
}

int cmd_logs(const char *log_path, int lines, int follow)
{
    if (!log_path || log_path[0] == '\0') {
        log_path = "/var/log/vigil/vigil.jsonl";
    }

    if (tail_log(log_path, lines) != 0)
        return 1;

    if (follow)
        return follow_log(log_path);

    return 0;
}
