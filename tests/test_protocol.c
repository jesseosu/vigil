/*
 * test_protocol.c — Unit tests for the IPC protocol
 *
 * Tests length-prefix encoding/decoding and validates protocol constants.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

#include "vigil/protocol.h"

#define TEST(name) do { printf("  [TEST] %s... ", #name); name(); printf("PASS\n"); } while(0)

static void test_encode_decode_zero(void)
{
    uint8_t buf[4];
    protocol_encode_length(buf, 0);
    assert(protocol_decode_length(buf) == 0);
}

static void test_encode_decode_small(void)
{
    uint8_t buf[4];
    protocol_encode_length(buf, 42);
    assert(protocol_decode_length(buf) == 42);
}

static void test_encode_decode_medium(void)
{
    uint8_t buf[4];
    protocol_encode_length(buf, 65535);
    assert(protocol_decode_length(buf) == 65535);
}

static void test_encode_decode_large(void)
{
    uint8_t buf[4];
    protocol_encode_length(buf, VIGIL_MAX_MSG_SIZE);
    assert(protocol_decode_length(buf) == VIGIL_MAX_MSG_SIZE);
}

static void test_encode_decode_max(void)
{
    uint8_t buf[4];
    protocol_encode_length(buf, 0xFFFFFFFF);
    assert(protocol_decode_length(buf) == 0xFFFFFFFF);
}

static void test_network_byte_order(void)
{
    uint8_t buf[4];

    /* 0x01020304 should be stored as [01][02][03][04] (big-endian) */
    protocol_encode_length(buf, 0x01020304);
    assert(buf[0] == 0x01);
    assert(buf[1] == 0x02);
    assert(buf[2] == 0x03);
    assert(buf[3] == 0x04);
}

static void test_roundtrip_various(void)
{
    uint32_t test_values[] = {
        0, 1, 127, 128, 255, 256, 1000, 4096,
        65535, 65536, 100000, 1000000, 0xDEADBEEF
    };

    for (size_t i = 0; i < sizeof(test_values) / sizeof(test_values[0]); i++) {
        uint8_t buf[4];
        protocol_encode_length(buf, test_values[i]);
        uint32_t decoded = protocol_decode_length(buf);
        assert(decoded == test_values[i]);
    }
}

static void test_constants(void)
{
    assert(VIGIL_HEADER_SIZE == 4);
    assert(VIGIL_MAX_MSG_SIZE == 64 * 1024);
    assert(VIGIL_PROTOCOL_VERSION == 1);
}

int main(void)
{
    printf("=== Protocol Tests ===\n");

    TEST(test_encode_decode_zero);
    TEST(test_encode_decode_small);
    TEST(test_encode_decode_medium);
    TEST(test_encode_decode_large);
    TEST(test_encode_decode_max);
    TEST(test_network_byte_order);
    TEST(test_roundtrip_various);
    TEST(test_constants);

    printf("\nAll protocol tests passed.\n");
    return 0;
}
