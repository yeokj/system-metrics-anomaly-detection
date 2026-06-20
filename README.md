# System Metrics Anomaly Detection

![Build Status](https://github.com/yeokj/system-metrics-anomaly-detection/actions/workflows/ci.yml/badge.svg)
![Docker](https://img.shields.io/badge/docker-%232496ED.svg?style=flat&logo=docker&logoColor=white)
![C++](https://img.shields.io/badge/c++-%2300599C.svg?style=flat&logo=c%2B%2B&logoColor=white)
![Python](https://img.shields.io/badge/python-3670A0?style=flat&logo=python&logoColor=ffdd54)
![PostgreSQL](https://img.shields.io/badge/postgres-%23316192.svg?style=flat&logo=postgresql&logoColor=white)
![gRPC](https://img.shields.io/badge/gRPC-%234285F4.svg?style=flat&logo=grpc&logoColor=white)

A high-performance infrastructure suite pairing an offline statistical training pipeline with an ultra-low-latency, multi-threaded C++ inference engine to ingest and validate system performance telemetry in real time. 

This architecture decouples heavy model computation from the execution path, using lock-free data structures and isolated thread boundaries to process streaming system metrics with minimal CPU overhead.

---

## 🏗️ System Architecture & Component Design

The platform is split into decoupled, dedicated microservices cooperating across an isolated network bridge:

1. **Analytical Pipeline (Python)**: Processes historical telemetry records, computes rolling windows, and trains statistical Z-score baseline thresholds. These parameters are serialized and exported to `model_params.json`.
2. **Telemetry Generator (C++)**: An independent simulation binary that synthesizes multi-threaded system telemetry (`latency`, `throughput`, `error_rate`) and streams messages over a client-side gRPC channel.
3. **Inference Server (C++)**: A high-performance gRPC server handling real-time ingestion. Incoming proto payloads are mapped into native objects and placed onto a lock-free queue.
4. **Persistent Audit Store (PostgreSQL)**: An infrastructure layer storing flagged anomaly alerts off the main inference execution path.