# Project Sentinel Architecture

## Overview

Project Sentinel is designed as a modular Linux telemetry framework focused on long-term system health monitoring and predictive maintenance.

Unlike traditional monitoring software that reports current system status, Sentinel continuously collects telemetry, stores historical information, analyses trends over time, and generates alerts when abnormal behaviour is detected.

The system follows a hybrid architecture consisting of two primary components:

- Kernel Component
- User-space Daemon

---

## High-Level Architecture

                    +----------------------+
                    |  Remote Dashboard    |
                    | REST / MQTT / gRPC   |
                    +----------+-----------+
                               |
                        Secure Communication
                               |
                    +----------v-----------+
                    |   Sentinel Daemon    |
                    |----------------------|
                    | Telemetry Database   |
                    | Alert Engine         |
                    | Trend Analysis       |
                    | Logging              |
                    | Configuration        |
                    +----------+-----------+
                               ^
                               |
                     Shared Telemetry API
                               |
                    +----------+-----------+
                    |  Kernel Component    |
                    |----------------------|
                    | Hardware Sensors     |
                    | Performance Counters |
                    | Low-level Monitoring |
                    +----------+-----------+
                               |
                  Linux Kernel Interfaces
                               |
        CPU • Memory • Storage • Network • Sensors

---

## Components

### Kernel Component

Responsibilities:

- Collect low-level telemetry
- Access hardware information
- Expose collected metrics to userspace
- Maintain minimal overhead
- Never perform expensive processing

The kernel component should remain as small as possible.

---

### User-space Daemon

Responsibilities:

- Receive telemetry
- Store historical data
- Perform trend analysis
- Generate alerts
- Handle configuration
- Communicate with remote systems
- Recover from failures

Most project functionality belongs here.

---

## Design Principles

Sentinel follows several core principles:

- Modular
- Lightweight
- Portable
- Fault tolerant
- Extensible
- Embedded-first
- Security conscious

---

## Directory Structure
sentinel/
├── daemon/
├── kernel/
├── common/
├── docs/
├── tests/
├── config/
├── scripts/
└── examples/


---

## Future Expansion

The architecture intentionally allows future additions such as:

- Machine learning
- Fleet management
- Web dashboard
- OTA diagnostics
- Additional operating systems