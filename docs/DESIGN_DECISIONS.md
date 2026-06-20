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