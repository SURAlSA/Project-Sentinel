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

Configuration uses a TOML-style file with the following supported settings:

Example:

```toml
sampling_interval_ms = 1000
database_path = "/var/lib/sentinel/data.db"
log_level = "info"
enable_remote_telemetry = false
```

The sampling interval must be from 100 through 60000 milliseconds. Log level
must be `debug`, `info`, `warn`, or `error`. Missing settings use defaults.
Unknown or duplicate settings and malformed values cause startup to fail.
Remote telemetry is not implemented yet; enabling it is currently diagnostic
configuration only.

---

## Versioning

Project Sentinel will follow Semantic Versioning.

Example:

1.0.0