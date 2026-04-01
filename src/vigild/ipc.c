/*
 * ipc.c — Unix domain socket IPC server
 *
 * AF_UNIX SOCK_STREAM server with poll() multiplexing.
 * Protocol: 4-byte length prefix (network byte order) + JSON payload.
 *
 * Handles commands: status, top, health, logs
 * Responds with JSON: {"ok": true, "data": {...}}
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <poll.h>
#include <time.h>

#include "ipc.h"
#include "log.h"
#include "vigil/protocol.h"

/* Parse the "cmd" field from a JSON request (minimal JSON parser) */
static ipc_cmd_t parse_command(const char *json)
{
    const char *p = strstr(json, "\"cmd\"");
    if (!p)
        return IPC_CMD_UNKNOWN;

    p = strchr(p + 4, ':');
    if (!p)
        return IPC_CMD_UNKNOWN;
    p++;

    while (*p == ' ' || *p == '\t')
        p++;

    if (*p == '"')
        p++;

    if (strncmp(p, "status", 6) == 0)
        return IPC_CMD_STATUS;
    if (strncmp(p, "top", 3) == 0)
        return IPC_CMD_TOP;
    if (strncmp(p, "health", 6) == 0)
        return IPC_CMD_HEALTH;
    if (strncmp(p, "logs", 4) == 0)
        return IPC_CMD_LOGS;

    return IPC_CMD_UNKNOWN;
}

/* Build a JSON status response from the latest sample */
static int build_status_response(char *buf, size_t bufsize,
                                 const ringbuf_t *rb)
{
    telemetry_sample_t sample;
    if (ringbuf_peek_latest(rb, &sample) < 0) {
        return snprintf(buf, bufsize,
            "{\"ok\":false,\"error\":\"no data available\"}");
    }

    char ts[64];
    struct tm tm;
    gmtime_r(&sample.timestamp.tv_sec, &tm);
    strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", &tm);

    return snprintf(buf, bufsize,
        "{\"ok\":true,\"data\":{"
        "\"timestamp\":\"%s.%03ldZ\","
        "\"cpu_user_pct\":%.1f,"
        "\"cpu_system_pct\":%.1f,"
        "\"cpu_iowait_pct\":%.1f,"
        "\"cpu_idle_pct\":%.1f,"
        "\"mem_used_pct\":%.1f,"
        "\"mem_available_mb\":%.1f,"
        "\"swap_used_pct\":%.1f,"
        "\"disk_read_bps\":%.0f,"
        "\"disk_write_bps\":%.0f,"
        "\"net_rx_bps\":%.0f,"
        "\"net_tx_bps\":%.0f,"
        "\"load_avg_1\":%.2f,"
        "\"load_avg_5\":%.2f,"
        "\"load_avg_15\":%.2f"
        "}}",
        ts, sample.timestamp.tv_nsec / 1000000,
        sample.cpu_user_pct, sample.cpu_system_pct,
        sample.cpu_iowait_pct, sample.cpu_idle_pct,
        sample.mem_used_pct, sample.mem_available_mb, sample.swap_used_pct,
        sample.disk_read_bps, sample.disk_write_bps,
        sample.net_rx_bps, sample.net_tx_bps,
        sample.load_avg_1, sample.load_avg_5, sample.load_avg_15);
}

/* Build health response */
static int build_health_response(char *buf, size_t bufsize,
                                 const ringbuf_t *rb,
                                 const vigil_config_t *cfg)
{
    telemetry_sample_t sample;
    int has_sample = (ringbuf_peek_latest(rb, &sample) == 0);

    int n = snprintf(buf, bufsize, "{\"ok\":true,\"data\":{\"metrics\":{");

    if (has_sample) {
        n += snprintf(buf + n, bufsize - (size_t)n,
            "\"cpu_pct\":%.1f,\"mem_pct\":%.1f,\"swap_pct\":%.1f",
            sample.cpu_user_pct + sample.cpu_system_pct,
            sample.mem_used_pct, sample.swap_used_pct);
    }

    n += snprintf(buf + n, bufsize - (size_t)n, "},\"rules\":[");

    for (int i = 0; i < cfg->n_rules && (size_t)n < bufsize - 100; i++) {
        if (i > 0)
            n += snprintf(buf + n, bufsize - (size_t)n, ",");
        n += snprintf(buf + n, bufsize - (size_t)n,
            "{\"name\":\"%s\",\"threshold\":%.1f,\"breaches\":%d}",
            cfg->rules[i].name, cfg->rules[i].threshold,
            cfg->rules[i].breach_count);
    }

    n += snprintf(buf + n, bufsize - (size_t)n, "]}}");
    return n;
}

/* Send a length-prefixed response */
static int send_response(int fd, const char *json, size_t json_len)
{
    uint8_t header[VIGIL_HEADER_SIZE];
    protocol_encode_length(header, (uint32_t)json_len);

    if (write(fd, header, VIGIL_HEADER_SIZE) != VIGIL_HEADER_SIZE)
        return -1;
    if (write(fd, json, json_len) != (ssize_t)json_len)
        return -1;

    return 0;
}

/* Handle a client request */
static void handle_client(int client_fd, ringbuf_t *rb,
                          const vigil_config_t *cfg)
{
    uint8_t header[VIGIL_HEADER_SIZE];
    ssize_t nread = read(client_fd, header, VIGIL_HEADER_SIZE);
    if (nread != VIGIL_HEADER_SIZE)
        return;

    uint32_t payload_len = protocol_decode_length(header);
    if (payload_len == 0 || payload_len > VIGIL_MAX_MSG_SIZE)
        return;

    char *request = malloc(payload_len + 1);
    if (!request)
        return;

    nread = read(client_fd, request, payload_len);
    if (nread != (ssize_t)payload_len) {
        free(request);
        return;
    }
    request[payload_len] = '\0';

    /* Parse command and build response */
    char response[VIGIL_MAX_MSG_SIZE];
    int resp_len = 0;

    ipc_cmd_t cmd = parse_command(request);
    free(request);

    switch (cmd) {
    case IPC_CMD_STATUS:
    case IPC_CMD_TOP:
        resp_len = build_status_response(response, sizeof(response), rb);
        break;

    case IPC_CMD_HEALTH:
        resp_len = build_health_response(response, sizeof(response), rb, cfg);
        break;

    case IPC_CMD_LOGS:
        resp_len = snprintf(response, sizeof(response),
            "{\"ok\":true,\"data\":{\"log_path\":\"%s\"}}",
            cfg->log_path);
        break;

    case IPC_CMD_UNKNOWN:
    default:
        resp_len = snprintf(response, sizeof(response),
            "{\"ok\":false,\"error\":\"unknown command\"}");
        break;
    }

    if (resp_len > 0)
        send_response(client_fd, response, (size_t)resp_len);
}

/* Remove a client from the array */
static void remove_client(ipc_server_t *srv, int idx)
{
    close(srv->client_fds[idx]);
    /* Shift remaining clients down */
    for (int i = idx; i < srv->n_clients - 1; i++)
        srv->client_fds[i] = srv->client_fds[i + 1];
    srv->n_clients--;
}

int ipc_init(ipc_server_t *srv, const char *socket_path)
{
    if (!srv || !socket_path)
        return -1;

    memset(srv, 0, sizeof(ipc_server_t));
    snprintf(srv->socket_path, sizeof(srv->socket_path), "%s", socket_path);

    /* Remove stale socket */
    unlink(socket_path);

    /* Create socket */
    srv->listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (srv->listen_fd < 0)
        return -1;

    /* Set non-blocking */
    int flags = fcntl(srv->listen_fd, F_GETFL, 0);
    fcntl(srv->listen_fd, F_SETFL, flags | O_NONBLOCK);

    /* Bind */
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", socket_path);

    if (bind(srv->listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(srv->listen_fd);
        return -1;
    }

    /* Restrict access */
    chmod(socket_path, 0660);

    /* Listen */
    if (listen(srv->listen_fd, 5) < 0) {
        close(srv->listen_fd);
        unlink(socket_path);
        return -1;
    }

    log_event(LOG_INFO, "ipc_listening",
              "path", socket_path, NULL);
    return 0;
}

int ipc_poll(ipc_server_t *srv, ringbuf_t *rb, const vigil_config_t *cfg,
             int timeout_ms)
{
    if (!srv)
        return -1;

    /* Build pollfd array: listener + clients */
    struct pollfd fds[1 + VIGIL_MAX_CLIENTS];
    int nfds = 0;

    fds[nfds].fd = srv->listen_fd;
    fds[nfds].events = POLLIN;
    fds[nfds].revents = 0;
    nfds++;

    for (int i = 0; i < srv->n_clients; i++) {
        fds[nfds].fd = srv->client_fds[i];
        fds[nfds].events = POLLIN;
        fds[nfds].revents = 0;
        nfds++;
    }

    int ret = poll(fds, (nfds_t)nfds, timeout_ms);
    if (ret <= 0)
        return 0;

    /* Check listener for new connections */
    if (fds[0].revents & POLLIN) {
        int client_fd = accept(srv->listen_fd, NULL, NULL);
        if (client_fd >= 0) {
            if (srv->n_clients < VIGIL_MAX_CLIENTS) {
                srv->client_fds[srv->n_clients++] = client_fd;
            } else {
                close(client_fd);  /* At capacity */
            }
        }
    }

    /* Check clients for data or disconnection */
    for (int i = srv->n_clients - 1; i >= 0; i--) {
        int fd_idx = i + 1;  /* offset by 1 for listener */

        if (fds[fd_idx].revents & (POLLHUP | POLLERR)) {
            remove_client(srv, i);
            continue;
        }

        if (fds[fd_idx].revents & POLLIN) {
            handle_client(srv->client_fds[i], rb, cfg);
            /* Close after handling (single request/response per connection) */
            remove_client(srv, i);
        }
    }

    return 0;
}

void ipc_cleanup(ipc_server_t *srv)
{
    if (!srv)
        return;

    for (int i = 0; i < srv->n_clients; i++)
        close(srv->client_fds[i]);
    srv->n_clients = 0;

    if (srv->listen_fd >= 0) {
        close(srv->listen_fd);
        srv->listen_fd = -1;
    }

    if (srv->socket_path[0] != '\0')
        unlink(srv->socket_path);
}
