# Design Decisions

## Philosophy

Project Sentinel is designed to answer one question:

> "Is this system becoming less healthy over time?"

Rather than focusing on real-time monitoring, Sentinel focuses on historical behaviour and long-term analysis.

---

## Guiding Principles

### Keep the kernel simple

The kernel should only collect data.

It should never:

- perform trend analysis
- perform machine learning
- maintain large databases
- communicate over the network

These responsibilities belong in userspace.

---

### Modular Design

Every subsystem should be independent.

Examples include:

- CPU monitoring
- Memory monitoring
- Disk monitoring
- Network monitoring
- Alerting
- Trend analysis

Modules should be removable or replaceable without affecting unrelated components.

---

### Reliability

Sentinel is intended for systems that may operate continuously for months or years.

Reliability takes priority over feature count.

---

### Security

The project follows the principle of least privilege wherever possible.

Remote telemetry should always be encrypted and authenticated.

---

### Performance

Goals include:

- CPU overhead below 2%
- Low memory usage
- Minimal disk writes
- Suitable for embedded hardware

---

## Coding Standards

- C17
- Consistent formatting
- Warning-free compilation
- Document public APIs
- Small focused functions
- Unit tests where practical

---

## Build System

Primary build tool:

- CMake

Kernel modules use the Linux kernel build system.