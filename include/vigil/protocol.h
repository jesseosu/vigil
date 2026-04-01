/*
 * vigil/protocol.h — IPC protocol definitions
 *
 * Wire format: [4 bytes uint32_t payload length, network byte order][JSON payload]
 *
 * Request:  {"cmd": "status"}
 *           {"cmd": "top", "interval": 1}
 *           {"cmd": "health"}
 *           {"cmd": "logs", "lines": 50}
 *
 * Response: {"ok": true, "data": { ... }}
 *           {"ok": false, "error": "..."}
 */

#ifndef VIGIL_PROTOCOL_H
#define VIGIL_PROTOCOL_H

#include <stdint.h>

#define VIGIL_PROTOCOL_VERSION   1
#define VIGIL_MAX_MSG_SIZE       (64 * 1024)   /* 64 KiB max message */
#define VIGIL_HEADER_SIZE        4              /* uint32_t length prefix */

/* IPC command identifiers (used internally after JSON parsing) */
typedef enum {
    IPC_CMD_STATUS = 0,
    IPC_CMD_TOP,
    IPC_CMD_HEALTH,
    IPC_CMD_LOGS,
    IPC_CMD_UNKNOWN
} ipc_cmd_t;

/*
 * Encode a 32-bit length in network byte order into buf.
 * buf must have at least 4 bytes.
 */
static inline void protocol_encode_length(uint8_t *buf, uint32_t len)
{
    buf[0] = (uint8_t)((len >> 24) & 0xFF);
    buf[1] = (uint8_t)((len >> 16) & 0xFF);
    buf[2] = (uint8_t)((len >> 8)  & 0xFF);
    buf[3] = (uint8_t)((len)       & 0xFF);
}

/*
 * Decode a 32-bit length from network byte order.
 */
static inline uint32_t protocol_decode_length(const uint8_t *buf)
{
    return ((uint32_t)buf[0] << 24) |
           ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] << 8)  |
           ((uint32_t)buf[3]);
}

#endif /* VIGIL_PROTOCOL_H */
