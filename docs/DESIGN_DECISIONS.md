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

---

## Phase 3: Ultra-Low Latency Optimization (Lock-Free)

### 1. Lock-Free SPSC Ring Buffer over Mutex Synchronization
* **Decision:** Replaced standard OS-level mutex and condition variable synchronization with a custom, template-based Single-Producer Single-Consumer (SPSC) Ring Buffer utilizing `std::atomic` pointers.
* **Rationale:** OS-level locks introduce severe thread scheduling contention, kernel context switches, and non-deterministic latency spikes at microsecond scales. Utilizing lock-free atomic `memory_order_acquire` and `memory_order_release` operations guarantees memory visibility across producer and consumer threads without ever blocking execution.

### 2. Hardware Cache Alignment (`alignas(64)`) to Eliminate False Sharing
* **Decision:** Enforced explicit 64-byte hardware cache boundary padding (`alignas(64)`) on atomic head/tail indexes and internal buffer structures.
* **Rationale:** In multi-core CPU architectures, adjacent variables sharing a single 64-byte L1 cache line cause hardware false sharing—where modifications by one thread force cache line invalidations on other cores. Isolating read and write offsets onto independent physical cache lines preserves L1/L2 cache locality and eliminates memory bus thrashing.

### 3. Asynchronous Network & Compute Decoupling (Strategy B)
* **Decision:** Refactored `InferenceServer` to isolate gRPC stream ingestion from machine learning evaluation into separate execution threads.
* **Rationale:** Performing computational validation or I/O directly inside the gRPC `StreamMetrics` thread forces socket read buffers to stall. Decoupling the pipeline allows incoming network packets to be pushed to the queue in nanoseconds, while a background consumer thread executes Z-score calculations asynchronously.

### 4. OS Scheduler Cooperation via `std::this_thread::yield()`
* **Decision:** Integrated microsecond CPU throttling in the background worker loop during queue underrun (empty state) conditions.
* **Rationale:** Tight busy-wait polling on empty lock-free queues pins CPU cores to 100% utilization. Yielding execution time slices back to the operating system scheduler prevents core pinning during idle stream windows without incurring heavy thread sleep latency.

### 5. Dynamic CLI Target Resolution for Hybrid Deployments
* **Decision:** Extended `telemetry_generator.cpp` with command-line argument parsing (`argc`/`argv`).
* **Rationale:** Hardcoding IP addresses creates brittle codebases. Enabling CLI target overrides allows the exact same client binary to execute against `localhost:50051` for fast local development loops or route across internal GCP VPC networks (`10.150.0.2:50051`) without re-compilation.

---

## Phase 4: Production Storage & Cloud Proxy Ingestion

### 1. Externalizing Credentials to Isolated Runtime Contexts
* **Decision:** Extracted the raw PostgreSQL connection URI from source control and migrated credential loading to dynamic runtime extraction via `std::getenv("SUPABASE_CONN_STR")`.
* **Rationale:** Hardcoding master database passwords inside source code violates fundamental security practices and leads to catastrophic secret leakage if pushed to open repositories. By extracting connection profiles entirely to environment contexts, the system becomes secure, platform-agnostic, and completely ready for stateless cloud deployment models like Docker or Kubernetes.

### 2. Transaction-Agnostic Query Architecture over Proxy Gates (PgBouncer)
* **Decision:** Removed session-pinned prepared statements (`c.prepare`) from thread boot contexts and replaced them with dynamic inline parameterized execution blocks (`write_tx.exec`) handled through `pqxx::params`.
* **Rationale:** Production cloud databases handle traffic surges by funneling TCP sockets through transaction pooling proxies (like PgBouncer on port `6543`). Because these proxies map multiple application connections to the same physical database session backend, trying to register a session-bound prepared statement across shared sessions throws a duplicate statement exception and terminates the database driver context. Utilizing inline parameterized wrappers ensures maximum throughput via PgBouncer while preserving total immunity against SQL injection attacks.

### 3. Idempotent Target Schema Self-Provisioning
* **Decision:** Embedded automatic DDL table execution blocks (`CREATE TABLE IF NOT EXISTS anomaly_alerts`) within the database worker initialization sequence.
* **Rationale:** Separating database setup scripts from application binary execution builds brittle deployment dependencies. By making storage layer schema definition fully idempotent and self-contained within the C++ source initialization sequence, the engine guarantees that database targets are verified, structured, and active the exact microsecond a WAN network socket link wakes up.

---

## Phase 5: Containerization & Continuous Deployment

### Decision 1: Multi-Stage Container Builds for C++ Microservices
* **Context**: Building C++ applications with gRPC and PostgreSQL client bindings requires heavy development packages (`build-essential`, `cmake`, `libgrpc++-dev`, `libpqxx-dev`). Standard single-stage container builds retain these compilation tools in the production image, introducing security vulnerabilities and bloated image sizes.
* **Alternative Considered**: Single-stage Ubuntu container housing both development tools and runtime execution paths.
* **Decision**: Adopted a multi-stage Docker compilation pattern (`ubuntu:24.04`). The `builder` stage compiles release binaries and Protobuf definitions from raw source, while the final `runtime` stage copies *only* the compiled binaries (`InferenceServer`, `TelemetryGenerator`) and serialized model parameters (`model_params.json`).
* **Rationale**: Decouples the development toolchain from the execution footprint, reducing attack vectors and ensuring lightweight runtime deployment.

### Decision 2: Automated Pipeline Orchestration & Container Registry Publishing
* **Context**: Manual binary compilation and local container tagging risk environment drift and broken builds reaching production repos.
* **Alternative Considered**: Local container builds manually pushed to Docker Hub or relying purely on local CMake execution.
* **Decision**: Engineered an automated GitHub Actions CI/CD pipeline (`.github/workflows/ci.yml`) featuring parallel linting (`flake8`), native C++ compilation verification, and automated Docker container deployment to GitHub Container Registry (`ghcr.io`).
* **Rationale**: Enforces strict automated validation gates on every commit/PR to `main`. Automatically tags, labels (OCI standards), and publishes immutable production packages directly to `ghcr.io`, eliminating manual release overhead and providing instant `docker pull` capabilities.

### Decision 3: Service Health Interlocks in Docker Compose
* **Context**: The gRPC C++ inference server requires active PostgreSQL socket endpoints to handle anomaly persistent logging upon startup. Launching containers in parallel causes race conditions where the inference server boots before the database daemon initializes.
* **Alternative Considered**: Application-level sleep loops or retry logic in C++ code.
* **Decision**: Configured explicit service dependency interlocks in `docker-compose.yml` using `depends_on` bound to a native PostgreSQL `healthcheck` (`pg_isready`).
* **Rationale**: Keeps database readiness logic at the infrastructure orchestration level rather than polluting application code, guaranteeing deterministic multi-container startup sequences.