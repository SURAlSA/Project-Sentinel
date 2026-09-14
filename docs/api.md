# Sentinel Internal API

This document describes communication between project components.

---

## Kernel → Daemon

The kernel component exposes telemetry through a shared interface.

Possible implementations:

- Netlink
- Character device
- Shared memory
- sysfs

The implementation will be evaluated during development.

---

## Daemon Modules

The daemon consists of several independent modules.

Examples:

- Telemetry Collector
- Database
- Alert Engine
- Trend Analysis
- Network Client
- Logger

Each module should expose a simple public interface.

---

## Remote API

Potential protocols include:

- REST
- MQTT
- WebSocket
- gRPC

The initial implementation will likely begin with REST before expanding to MQTT support.

---

## Configuration

Configuration will use TOML.

Example:

```toml
sampling_interval = 1000

database = "/var/lib/sentinel/data.db"

enable_network = true
```

---

## Versioning

Project Sentinel will follow Semantic Versioning.

Example:

1.0.0