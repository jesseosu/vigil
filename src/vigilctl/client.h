/*
 * client.h — Unix domain socket client for vigilctl
 *
 * Connects to the vigild daemon socket, sends length-prefixed JSON
 * requests, and receives length-prefixed JSON responses.
 */

#ifndef VIGILCTL_CLIENT_H
#define VIGILCTL_CLIENT_H

#include <stddef.h>

/* Connect to the vigild Unix socket. Returns fd on success, -1 on error. */
int client_connect(const char *socket_path);

/*
 * Send a JSON request over the socket (with length prefix).
 * Returns 0 on success, -1 on error.
 */
int client_send(int fd, const char *json, size_t json_len);

/*
 * Receive a JSON response from the socket (with length prefix).
 * Allocates the response buffer — caller must free().
 * Returns the response length, or -1 on error.
 */
int client_recv(int fd, char **response);

/* Close the connection. */
void client_close(int fd);

#endif /* VIGILCTL_CLIENT_H */
