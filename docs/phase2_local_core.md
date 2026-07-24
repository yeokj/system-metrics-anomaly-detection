# Engineering Journal - Phase 2: Distributed Systems Ingestion & GCP Cloud Deployment

## 1. Overview
The core objective of Phase 2 was to transition the anomaly detection engine from a local, single-process execution model into a distributed cloud architecture. By breaking the system into isolated executables running across independent Google Cloud Platform (GCP) Compute Engine Virtual Machines, the pipeline eliminated local in-process memory assumptions and established a scalable, streaming network layer powered by gRPC and Protocol Buffers.

---

## 2. The Bottlenecks & Architectural Problems
While Phase 1 secured local thread safety and memory bounds, scaling the application across isolated physical or virtual boundaries introduced several critical architectural challenges:

* **In-Process Architectural Coupling:** The telemetry simulation and detection engines originally shared local process memory. This made it impossible to simulate real-world infrastructure monitoring where telemetry originates on remote hosts and streams across a network to a centralized inference engine.
* **High Network Overhead & Payload Bloat:** Traditional JSON-over-HTTP REST APIs introduce significant serialization costs, text parsing overhead, and high transport latency, making them unsuitable for real-time telemetry streaming at microsecond scales.
* **Environment & Cloud Isolation Barriers:** Streaming metrics across cloud boundaries required provisioning isolated cloud infrastructure, configuring internal Virtual Private Cloud (VPC) network firewall rules, and resolving environment-specific compilation differences across different Linux runtime environments.

---

## 3. Technical Implementations & The "Why"

### A. Binary Decoupling & Protocol Buffer Schema Design
* **The Action:** Split the monolithic architecture into two completely independent targets: `TelemetryGenerator` (the remote client application) and `InferenceServer` (the centralized gRPC processing server). Defined a strict Protocol Buffer schema (`metrics.proto`) utilizing gRPC client-side streaming (`StreamMetrics`).
* **The Rationale:** Protocol Buffers provide compact binary serialization and zero-copy parsing, vastly outperforming JSON in both bandwidth utilization and CPU efficiency. Client-side streaming allows the generator to keep a single persistent TCP connection open, avoiding the high overhead of establishing repeated HTTP connections for every telemetry frame.

### B. Dynamic CMake Dependency Resolution
* **The Action:** Enhanced `CMakeLists.txt` with flexible gRPC discovery logic. The script first attempts to locate gRPC via native CMake Config files (`find_package(gRPC CONFIG QUIET)`). If missing (common on Ubuntu `apt` distributions), it seamlessly falls back to `PkgConfig` (`pkg_check_modules(GRPC REQUIRED grpc++)`) and dynamically locates `grpc_cpp_plugin` on the host system.
* **The Rationale:** Ensures deterministic cross-environment compilation across local macOS/Linux workstations, automated CI runners, and GCP cloud VM instances without requiring hardcoded compiler paths or OS-specific tweaks.

### C. GCP Infrastructure & VPC Network Provisioning
* **The Action:** Provisioned two isolated Linux Virtual Machines on Google Compute Engine. Configured custom GCP VPC firewall rules to allow high-performance streaming traffic on port `50051` between internal IP addresses (`10.150.0.2:50051`).
* **The Rationale:** Isolating client generation from server inference models real-world distributed infrastructure deployment and forces the anomaly detector to handle true network transport semantics, latency, and socket lifecycles.

---

## 4. Challenges & Engineering Troubleshooting

### A. gRPC Header Incompatibilities & Type Declarations
* **The Challenge:** During initial server compilation, GCC threw severe error cascades regarding template conflicts between explicit forward declarations in custom header files (`namespace grpc { template <typename W> class ClientWriter; }`) and gRPC's internal library template types (`grpc_impl::ClientWriter<W>`), resulting in invalid covariant return types in the generated `.grpc.pb.h` code.
* **The Solution:** Refactored header includes to replace forward declarations with native gRPC includes (`<grpcpp/grpcpp.h>`) and updated function signatures to use fully qualified namespace declarations (`grpc::ClientWriter<telemetry::TelemetryMetric>*`).

### B. Runtime Environment Execution & Configuration Scoping
* **The Challenge:** Executing binaries outside the repository root triggered file-not-found errors when loading machine learning parameters (`Error: Could not open config file at python/data/model_params.json`).
* **The Solution:** Standardized directory execution contexts across both VMs by configuring working directories and creating relative symlinks (`ln -s ~/system-metrics-anomaly-detection/python python`), ensuring the C++ detection engine reliably accesses model parameters regardless of invocation path.

### C. Destructive Shell Recovery & Workspace Realignment
* **The Challenge:** An accidental wildcard deletion command (`rm -rf *`) wiped local build artifacts and uncommitted files on the server VM during configuration testing.
* **The Solution:** Re-cloned the repository from remote Git origin, re-executed the automated CMake generation sequence, re-linked python assets, and established safe `.gitignore` boundaries to protect source files from untracked build artifacts.

---

## 5. Architectural Proof & Verification

Upon completing Phase 2 deployment, the distributed cloud system demonstrated the following verifiable milestones:

1. **Distributed Cloud Ingestion:** Successfully established persistent gRPC client-streaming from `TelemetryGenerator` on VM-1 to `InferenceServer` on VM-2 across internal GCP VPC networking (`10.150.0.2:50051`).
2. **Deterministic Payload Streaming:** Processed 100 consecutive streaming telemetry evaluations with zero packet drops, clean client-side stream termination, and safe server shutdown on signal interrupt (`SIGINT`).
3. **Network Latency Baseline:** Benchmarked network transmission overhead, establishing a baseline CPU overhead profile of **`19.9 μs`** per metric evaluation over the network.