/*
 * client.c — Unix domain socket client
 *
 * Implements the IPC client for vigilctl. Connects to the vigild daemon's
 * Unix socket, sends/receives length-prefixed JSON messages.
 * Timeout: 5 seconds via SO_RCVTIMEO.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>

#include "client.h"
#include "vigil/protocol.h"

int client_connect(const char *socket_path)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;

    /* Set receive timeout to 5 seconds */
    struct timeval tv;
    tv.tv_sec  = 5;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", socket_path);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

int client_send(int fd, const char *json, size_t json_len)
{
    uint8_t header[VIGIL_HEADER_SIZE];
    protocol_encode_length(header, (uint32_t)json_len);

    if (write(fd, header, VIGIL_HEADER_SIZE) != VIGIL_HEADER_SIZE)
        return -1;
    if (write(fd, json, json_len) != (ssize_t)json_len)
        return -1;

    return 0;
}

int client_recv(int fd, char **response)
{
    uint8_t header[VIGIL_HEADER_SIZE];
    ssize_t nread = read(fd, header, VIGIL_HEADER_SIZE);
    if (nread != VIGIL_HEADER_SIZE)
        return -1;

    uint32_t payload_len = protocol_decode_length(header);
    if (payload_len == 0 || payload_len > VIGIL_MAX_MSG_SIZE)
        return -1;

    char *buf = malloc(payload_len + 1);
    if (!buf)
        return -1;

    nread = read(fd, buf, payload_len);
    if (nread != (ssize_t)payload_len) {
        free(buf);
        return -1;
    }

    buf[payload_len] = '\0';
    *response = buf;
    return (int)payload_len;
}

void client_close(int fd)
{
    if (fd >= 0)
        close(fd);
}
