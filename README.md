# Project Sentinel

> Lightweight autonomous telemetry and predictive health monitoring for Linux systems.

---

## Overview

Project Sentinel is an open-source telemetry framework designed for Linux-based embedded and mission-critical systems.

Unlike traditional monitoring software that focuses on current system status, Sentinel continuously collects telemetry, stores historical data, analyses long-term trends, and identifies signs of system degradation before failures occur.

The project follows a hybrid architecture consisting of a lightweight Linux kernel component and a userspace daemon.

---

## Goals

Sentinel aims to:

- Monitor hardware health
- Monitor operating system behaviour
- Record historical telemetry
- Detect long-term degradation
- Generate alerts
- Support secure remote monitoring
- Operate with minimal resource usage
- Run autonomously

---

## Architecture

```
Applications
     ▲
REST / MQTT
     ▲
Sentinel Daemon
     ▲
Shared Interface
     ▲
Kernel Component
     ▲
Linux Kernel
     ▲
Hardware
```

---

## Planned Features

### Hardware

- CPU
- GPU
- Memory
- Storage
- SMART
- Power
- Thermal
- Battery

### Operating System

- Processes
- Services
- Boot history
- Kernel events
- OOM events

### Networking

- Throughput
- Latency
- Packet loss
- Interface health

### Analysis

- Historical telemetry
- Trend analysis
- Alert generation
- Anomaly detection

---

## Technology Stack

| Component | Technology |
|------------|------------|
| Language | C17 |
| Build System | CMake |
| Kernel Module | Linux Kernel Module |
| Database | SQLite (planned) |
| Testing | CTest |
| Documentation | Markdown + Doxygen |
| CI | GitHub Actions |

---

## Project Structure

```
sentinel/
├── daemon/
├── kernel/
├── common/
├── docs/
├── tests/
├── config/
├── scripts/
└── examples/
```

---

## Development Roadmap

- Repository setup
- Basic daemon
- Kernel module
- Telemetry collection
- Database
- Alert engine
- Remote telemetry
- Trend analysis

See the documentation in `docs/` for detailed design information.

---

## Building

Sentinel uses an out-of-tree CMake build:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

The daemon can be run in the foreground while development is underway:

```sh
./build/daemon/sentineld --config config/sentinel.toml
```

The kernel component is a separate Linux kernel module and must be built with
the kernel build system. See [`kernel/Makefile`](kernel/Makefile).

---

## License

MIT License

---

## Status

Early Development

The project is currently in its initial planning and architecture stage.