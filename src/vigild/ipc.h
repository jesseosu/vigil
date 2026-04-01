/*
 * ipc.h — Unix domain socket IPC server
 *
 * Provides a non-blocking socket server that vigilctl connects to.
 * Uses poll() for multiplexing client connections.
 * Protocol: length-prefixed JSON messages.
 */

#ifndef VIGILD_IPC_H
#define VIGILD_IPC_H

#include "ringbuf.h"
#include "config.h"

/* IPC server state */
typedef struct {
    int listen_fd;
    int client_fds[16];  /* VIGIL_MAX_CLIENTS */
    int n_clients;
    char socket_path[256];
} ipc_server_t;

/* Initialize the IPC server: create socket, bind, listen. Returns 0 on success. */
int ipc_init(ipc_server_t *srv, const char *socket_path);

/*
 * Poll for IPC activity: accept new connections, read requests, send responses.
 * timeout_ms: poll timeout in milliseconds (100ms recommended).
 * Returns 0 on success.
 */
int ipc_poll(ipc_server_t *srv, ringbuf_t *rb, const vigil_config_t *cfg,
             int timeout_ms);

/* Clean up: close all connections, remove socket file. */
void ipc_cleanup(ipc_server_t *srv);

#endif /* VIGILD_IPC_H */
