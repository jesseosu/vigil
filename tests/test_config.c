/*
 * test_config.c — Unit tests for the TOML configuration parser
 *
 * Creates a temporary TOML file, parses it, and verifies all fields.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <math.h>

#include "vigild/config.h"

#define TEST(name) do { printf("  [TEST] %s... ", #name); name(); printf("PASS\n"); } while(0)

static const char *TEST_CONFIG =
    "# Test configuration\n"
    "\n"
    "[daemon]\n"
    "tick_interval = 2\n"
    "ring_capacity = 1800\n"
    "pid_file = \"/tmp/vigil_test.pid\"\n"
    "socket_path = \"/tmp/vigil_test.sock\"\n"
    "log_path = \"/tmp/vigil_test.jsonl\"\n"
    "\n"
    "[collectors]\n"
    "disk_devices = [\"sda\", \"nvme0n1\"]\n"
    "net_interfaces = [\"eth0\", \"lo\"]\n"
    "\n"
    "[[watchdog.rules]]\n"
    "name = \"high_cpu\"\n"
    "metric = \"cpu\"\n"
    "threshold = 90.0\n"
    "sustained_seconds = 30\n"
    "action = \"alert\"\n"
    "cooldown_seconds = 300\n"
    "\n"
    "[[watchdog.rules]]\n"
    "name = \"mem_pressure\"\n"
    "metric = \"mem\"\n"
    "threshold = 95.0\n"
    "sustained_seconds = 10\n"
    "action = \"restart\"\n"
    "action_target = \"myapp\"\n"
    "cooldown_seconds = 60\n"
    "\n"
    "[[watchdog.processes]]\n"
    "name = \"nginx\"\n"
    "action = \"restart\"\n"
    "action_target = \"nginx\"\n"
    "cooldown_seconds = 30\n";

static char g_tmpfile[256];

static void create_tmpfile(void)
{
    snprintf(g_tmpfile, sizeof(g_tmpfile), "/tmp/vigil_test_config_XXXXXX");
    int fd = mkstemp(g_tmpfile);
    assert(fd >= 0);
    ssize_t ret = write(fd, TEST_CONFIG, strlen(TEST_CONFIG));
    (void)ret;
    close(fd);
}

static void cleanup_tmpfile(void)
{
    unlink(g_tmpfile);
}

static void test_defaults(void)
{
    vigil_config_t cfg;
    config_defaults(&cfg);

    assert(cfg.tick_interval == 1);
    assert(cfg.ring_capacity == 3600);
    assert(strcmp(cfg.pid_file, "/var/run/vigil.pid") == 0);
    assert(cfg.n_rules == 0);
    assert(cfg.n_processes == 0);

    config_free(&cfg);
}

static void test_parse_daemon_section(void)
{
    create_tmpfile();

    vigil_config_t cfg;
    assert(config_load(&cfg, g_tmpfile) == 0);

    assert(cfg.tick_interval == 2);
    assert(cfg.ring_capacity == 1800);
    assert(strcmp(cfg.pid_file, "/tmp/vigil_test.pid") == 0);
    assert(strcmp(cfg.socket_path, "/tmp/vigil_test.sock") == 0);
    assert(strcmp(cfg.log_path, "/tmp/vigil_test.jsonl") == 0);

    config_free(&cfg);
    cleanup_tmpfile();
}

static void test_parse_collectors(void)
{
    create_tmpfile();

    vigil_config_t cfg;
    assert(config_load(&cfg, g_tmpfile) == 0);

    assert(cfg.n_disk_devices == 2);
    assert(strcmp(cfg.disk_devices[0], "sda") == 0);
    assert(strcmp(cfg.disk_devices[1], "nvme0n1") == 0);

    assert(cfg.n_net_interfaces == 2);
    assert(strcmp(cfg.net_interfaces[0], "eth0") == 0);
    assert(strcmp(cfg.net_interfaces[1], "lo") == 0);

    config_free(&cfg);
    cleanup_tmpfile();
}

static void test_parse_rules(void)
{
    create_tmpfile();

    vigil_config_t cfg;
    assert(config_load(&cfg, g_tmpfile) == 0);

    assert(cfg.n_rules == 2);

    /* First rule */
    assert(strcmp(cfg.rules[0].name, "high_cpu") == 0);
    assert(cfg.rules[0].metric == METRIC_CPU);
    assert(fabs(cfg.rules[0].threshold - 90.0) < 0.001);
    assert(cfg.rules[0].sustained_seconds == 30);
    assert(cfg.rules[0].action == ACTION_ALERT);
    assert(cfg.rules[0].cooldown_seconds == 300);

    /* Second rule */
    assert(strcmp(cfg.rules[1].name, "mem_pressure") == 0);
    assert(cfg.rules[1].metric == METRIC_MEM);
    assert(fabs(cfg.rules[1].threshold - 95.0) < 0.001);
    assert(cfg.rules[1].action == ACTION_RESTART);
    assert(strcmp(cfg.rules[1].action_target, "myapp") == 0);

    config_free(&cfg);
    cleanup_tmpfile();
}

static void test_parse_processes(void)
{
    create_tmpfile();

    vigil_config_t cfg;
    assert(config_load(&cfg, g_tmpfile) == 0);

    assert(cfg.n_processes == 1);
    assert(strcmp(cfg.processes[0].name, "nginx") == 0);
    assert(cfg.processes[0].action == ACTION_RESTART);
    assert(strcmp(cfg.processes[0].action_target, "nginx") == 0);
    assert(cfg.processes[0].cooldown_seconds == 30);

    config_free(&cfg);
    cleanup_tmpfile();
}

static void test_reload(void)
{
    create_tmpfile();

    vigil_config_t cfg;
    assert(config_load(&cfg, g_tmpfile) == 0);
    assert(cfg.tick_interval == 2);

    /* Reload should produce the same result */
    assert(config_reload(&cfg, g_tmpfile) == 0);
    assert(cfg.tick_interval == 2);
    assert(cfg.n_rules == 2);

    config_free(&cfg);
    cleanup_tmpfile();
}

static void test_missing_file(void)
{
    vigil_config_t cfg;
    assert(config_load(&cfg, "/nonexistent/path/vigil.toml") == -1);
}

int main(void)
{
    printf("=== Config Parser Tests ===\n");

    TEST(test_defaults);
    TEST(test_parse_daemon_section);
    TEST(test_parse_collectors);
    TEST(test_parse_rules);
    TEST(test_parse_processes);
    TEST(test_reload);
    TEST(test_missing_file);

    printf("\nAll config parser tests passed.\n");
    return 0;
}
