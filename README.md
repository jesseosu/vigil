# Vigil

**Lightweight Linux system monitoring and self-healing daemon**

Vigil is a daemon written in C that reads from `/proc` and `/sys` to collect real-time telemetry, detect anomalies, and automatically remediate issues. It includes a companion CLI tool (`vigilctl`) that communicates with the daemon over a Unix domain socket.

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                     Linux kernel                         │
│              /proc  ·  /sys  ·  netlink                  │
└──────────────────────────┬──────────────────────────────┘
                           │ reads (every tick)
┌──────────────────────────▼──────────────────────────────┐
│                    vigild (daemon)                        │
│                                                          │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌───────────┐  │
│  │   CPU    │ │  Memory  │ │   Disk   │ │  Network  │  │
│  │/proc/stat│ │/proc/    │ │/proc/    │ │/proc/net/ │  │
│  │          │ │ meminfo  │ │diskstats │ │   dev     │  │
│  └────┬─────┘ └────┬─────┘ └────┬─────┘ └─────┬─────┘  │
│       └─────────────┴───────────┴──────────────┘        │
│                          │                               │
│              ┌───────────▼───────────┐                   │
│              │  Telemetry ring buffer │                   │
│              │  (fixed-size, in-mem)  │                   │
│              └─────┬───────────┬─────┘                   │
│                    │           │                          │
│         ┌──────────▼──┐  ┌────▼──────────────┐          │
│         │  Watchdog   │  │ Unix socket server │          │
│         │  engine     │  │ /var/run/vigil.sock│          │
│         └──────┬──────┘  └────────┬───────────┘          │
│                │                  │                       │
│     ┌──────────┼──────────┐      │                       │
│     ▼          ▼          ▼      │                       │
│  restart    alert     run script │                       │
│  service    (log)     (custom)   │                       │
│                                  │                       │
└──────────────────────────────────┼───────────────────────┘
                                   │ Unix socket IPC
                    ┌──────────────▼───────────────┐
                    │        vigilctl (CLI)          │
                    │  status · top · health · logs  │
                    └───────────────────────────────┘
```

## Build

```bash
cd vigil
make            # Build vigild and vigilctl
make test       # Run unit tests
make install    # Install to /usr/local/bin + /etc/vigil + systemd
make clean      # Remove build artifacts
```

**Requirements:** GCC with C11 support, Linux with procfs. No external dependencies.

## Usage

### Daemon

```bash
# Foreground mode (for testing)
vigild -f -c vigil.toml.example

# As a systemd service
sudo cp vigil.toml.example /etc/vigil/vigil.toml
sudo systemctl daemon-reload
sudo systemctl start vigil
sudo systemctl enable vigil
```

### CLI

```bash
vigilctl status              # One-shot system snapshot
vigilctl top --interval 2    # Live-updating display (like htop-lite)
vigilctl health              # Pass/fail per monitored service/process
vigilctl logs --lines 50     # Tail structured log
vigilctl logs --follow       # Follow log file in real-time
```

### Configuration

Copy `vigil.toml.example` to `/etc/vigil/vigil.toml` and adjust:

```toml
[daemon]
tick_interval = 1          # seconds
ring_capacity = 3600       # samples (1 hour)

[collectors]
disk_devices = ["sda"]
net_interfaces = ["eth0"]

[[watchdog.rules]]
name = "high_cpu"
metric = "cpu"
threshold = 90.0
sustained_seconds = 30
action = "alert"
cooldown_seconds = 300

[[watchdog.processes]]
name = "nginx"
action = "restart"
action_target = "nginx"
cooldown_seconds = 30
```

Reload config without restart: `kill -HUP $(cat /var/run/vigil.pid)`

## External Files

| Path | Purpose |
|------|---------|
| `/etc/vigil/vigil.toml` | Configuration (thresholds, services, alerts) |
| `/var/log/vigil/vigil.jsonl` | Structured JSON logs |
| `/var/run/vigil.pid` | PID file |
| `/var/run/vigil.sock` | Unix domain socket |
| `/etc/systemd/system/vigil.service` | systemd unit file |

## Design Decisions

### Why C instead of C++ or Rust?

Closest to the kernel, minimal runtime, direct syscall access. A monitoring daemon should be as lightweight as possible -- no STL overhead, no runtime. Also demonstrates comfort with manual memory management.

### Why a ring buffer instead of a growable array or linked list?

Fixed memory footprint (no allocation after init), cache-friendly sequential access, O(1) push. A monitoring daemon with unbounded memory growth would be ironic.

### Why Unix domain sockets instead of TCP?

Local-only communication (CLI to daemon on same host), no network overhead, filesystem-based access control (chmod on socket file), no need for a port.

### Why poll() instead of epoll?

Small number of concurrent connections (CLI clients, not thousands). poll() is simpler, portable, and sufficient. epoll would be premature optimisation for <=10 fds.

### Why hand-rolled TOML parser instead of a library?

Demonstrates parsing skills, avoids external dependencies, keeps the binary self-contained. Only a subset of TOML is needed.

### Why JSON logs instead of syslog?

Structured, greppable, parseable by any tool. Easy to ingest into log aggregation systems. syslog is an option but loses structure.

### Why forking daemon instead of Type=simple systemd service?

Demonstrates understanding of the traditional Unix daemon lifecycle: double-fork, setsid, PID file. Also works on systems without systemd.

## Project Structure

```
vigil/
├── Makefile
├── README.md
├── LICENSE                     (MIT)
├── vigil.toml.example
├── vigil.service
├── src/
│   ├── vigild/                 (daemon)
│   │   ├── main.c              Entry point, main loop
│   │   ├── daemon.c/h          Daemonisation, PID file, signals
│   │   ├── collector.c/h       Collector dispatcher
│   │   ├── collector_cpu.c     CPU: /proc/stat
│   │   ├── collector_mem.c     Memory: /proc/meminfo
│   │   ├── collector_disk.c    Disk: /proc/diskstats
│   │   ├── collector_net.c     Network: /proc/net/dev
│   │   ├── ringbuf.c/h         Telemetry ring buffer
│   │   ├── watchdog.c/h        Threshold checks, anomaly detection
│   │   ├── action.c/h          Remediation actions
│   │   ├── ipc.c/h             Unix socket server
│   │   ├── config.c/h          TOML parser
│   │   └── log.c/h             Structured JSON logging
│   └── vigilctl/               (CLI)
│       ├── main.c              Argument parsing, dispatch
│       ├── client.c/h          Socket client
│       ├── cmd_status.c        Status display
│       ├── cmd_top.c           Live-updating top
│       ├── cmd_health.c        Health check
│       └── cmd_logs.c          Log viewer
├── include/vigil/
│   ├── types.h                 Shared types
│   └── protocol.h              IPC protocol
└── tests/
    ├── test_ringbuf.c
    ├── test_collector_cpu.c
    ├── test_config.c
    └── test_protocol.c
```

## License

MIT -- see [LICENSE](LICENSE).
