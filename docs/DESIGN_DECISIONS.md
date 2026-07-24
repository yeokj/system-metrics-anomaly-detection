# System Architecture Design Decisions

## Phase 1: Robust Local Core & Clean Up

### 1. Transition from Research Data to Production Telemetry
* **Decision:** Renamed the data layer structures from generic names (`Metric`) to `TelemetryMetric` and moved away from academic console naming.
* **Rationale:** Establishes a production-ready enterprise domain language. It clearly signals that the application processes streaming hardware monitoring indicators (latency, throughput, error rates) rather than static data analysis sets.

### 2. Lock Lifespan Optimization via Block-Scoped RAII
* **Decision:** Migrated the consumer thread loop synchronization from explicit, manual `.unlock()` triggers to an isolated, localized block scope wrapper `{ ... }`.
* **Rationale:** In-process concurrency is brittle. Relying on manual mutex tracking leaves code highly vulnerable to permanent deadlocks or resource leaks if downstream logic triggers unexpected early returns or exceptions. Encapsulating the `std::unique_lock` and condition variable wait exclusively within bare inner braces guarantees immediate, automated resource cleanup exactly when execution exits the queue extraction block. This safely shrinks our lock granularity to the minimum critical path, leaving the heavy analytical model execution entirely unlocked.

### 3. Build Orchestration via Native Generation (CMake)
* **Decision:** Replaced rigid command-line compilation strings with a standard `CMakeLists.txt` generation script enforcing strict `C++17` standards.
* **Rationale:** Mitigates host-machine dependency breaks by fully automating system compiler flags, directory discovery, and operating system multi-threading library linking (`pthreads`). This guarantees that the core engine compiles and links identically across Linux, macOS, or automated integration containers.

---

## Phase 2: Distributed Systems Ingestion & GCP Cloud Deployment

### 1. gRPC & Protocol Buffers over JSON/HTTP REST
* **Decision:** Replaced local in-process queue mechanisms with gRPC streaming RPCs driven by Protocol Buffers schema definitions (`metrics.proto`).
* **Rationale:** Protocol Buffers provide compact binary serialization and zero-copy parsing, outperforming text-based JSON formats. Utilizing gRPC client-side streaming keeps a single persistent TCP socket open between services, avoiding the overhead of establishing new HTTP handshakes per metric payload and maintaining sub-microsecond transmission targets.

### 2. Standalone Binary Decoupling
* **Decision:** Decoupled monolithic executable logic into two isolated binaries: `TelemetryGenerator` (streaming client) and `InferenceServer` (anomaly detection receiver).
* **Rationale:** Decoupling telemetry generation from real-time model inference mirrors real-world cloud infrastructure monitoring. It eliminates reliance on shared in-process memory and allows telemetry emitters and inference servers to scale independently.

### 3. Isolated Multi-VM Cloud Topography on GCP
* **Decision:** Provisioned two Google Compute Engine VMs in a isolated Google Cloud Platform VPC network communicating via port `50051` (`10.150.0.2:50051`).
* **Rationale:** Testing across isolated cloud nodes forces the system to handle real-world network transport dynamics, latency overhead (benchmarked at `19.9 μs`), socket lifecycle events, and directory path scoping across remote host environments.

### 4. Dynamic Fallback Dependency Resolution in CMake
* **Decision:** Extended `CMakeLists.txt` to attempt gRPC discovery via native CMake config packages (`find_package(gRPC CONFIG)`) with automated fallbacks to `PkgConfig` (`pkg_check_modules`).
* **Rationale:** Eliminates brittle, host-dependent library paths. Ensures seamless cross-environment builds regardless of whether gRPC is installed via Homebrew on macOS, custom source compiles, or system `apt` packages on Linux VM instances.