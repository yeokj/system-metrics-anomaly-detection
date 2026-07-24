# Phase 4: Production Storage & Public Cloud Synchronization

## 1. Executive Summary & Objective
The primary goal of Phase 4 was to transition the monitoring platform from an in-memory, ephemeral anomaly detector into a persistent, enterprise-grade distributed system. By introducing native C++ relational database synchronization (`libpqxx`), the application now asynchronously offloads real-time anomaly alerts across public WAN links into a managed PostgreSQL cloud instance (Supabase) without sacrificing the sub-3 microsecond edge inference hot-path.

---

## 2. Technical Architecture & Engineering Decisions

### A. Non-Blocking Thread Isolation (gRPC Hot-Path Insulation)
* **Design:** Database writes incur unpredictable network WAN latency (15 ms to 80+ ms per round trip). To protect the ultra-low latency gRPC ingestion thread from blocking I/O, alert records are immediately pushed onto an in-memory Single-Producer Single-Consumer (SPSC) ring buffer queue.
* **Worker Loop:** A dedicated background worker thread (`dbWorkerLoop`) continuously drains the queue and manages WAN transaction life cycles asynchronously.

### B. Transaction Pooler Compatibility (PgBouncer)
* **Challenge:** Cloud database services multiplex incoming TCP connections using transaction proxies (PgBouncer on port `6543`). Initial implementation attempts using session-pinned prepared statements (`c.prepare`) threw duplicate statement exceptions (`ERROR: prepared statement "insert_alert" already exists`) when backend database sessions were reused across client connections.
* **Resolution:** Refactored query execution from pre-compiled session statements to inline parameterized transactions (`write_tx.exec("...", pqxx::params{...})`). This completely resolved proxy context conflicts while maintaining strict immunity against SQL injection vulnerabilities.

### C. Zero-Leak Credential Architecture
* **Security:** Hardcoding master database passwords or URIs inside source control creates severe security risks.
* **Implementation:** Database connection profiles were completely externalized using dynamic runtime environment extraction (`std::getenv("SUPABASE_CONN_STR")`). The engine dynamically parses connection parameters and enforces SSL encryption flags (`sslmode=require`) prior to opening socket tunnels.

### D. Idempotent Target Schema Provisioning
* **Deployment Automation:** To enable seamless multi-container deployments in future phases, table generation logic (`CREATE TABLE IF NOT EXISTS anomaly_alerts`) was embedded directly into the worker initialization lifecycle.

---

## 3. Storage Schema Specification

Alert events are synchronized to PostgreSQL using the following structured relational schema:

```sql
CREATE TABLE IF NOT EXISTS anomaly_alerts (
    id SERIAL PRIMARY KEY,
    timestamp BIGINT NOT NULL,
    metric_type TEXT NOT NULL,
    violated_value DOUBLE PRECISION NOT NULL,
    threshold_boundary DOUBLE PRECISION NOT NULL,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP
);

---

## 4. Challenges & Operational Resolutions

| Challenge / Bottleneck | Root Cause | Engineering Solution |
| :--- | :--- | :--- |
| **Path Resolution Failure** | Executing binaries from `cpp/build` prevented resolution of relative path `python/data/model_params.json`. | Configured environment execution relative to the project root directory. |
| **`libpqxx` C++ Standard Mismatch** | Modern `libpqxx` templates require C++20 features not enabled in base C++17 CMake scripts. | Upgraded `CMakeLists.txt` project standard to `CMAKE_CXX_STANDARD 20` and added explicit Apple Silicon library lookups. |
| **Prepared Statement Panics** | PgBouncer transaction pooling reuses underlying session handles, triggering duplicate prepared query registration errors. | Replaced session `prepare()` routines with inline parameterized transaction execution via `pqxx::params`. |

---

## 5. Empirical Performance & Stress Benchmarks

To validate thread isolation and storage reliability, an end-to-end stress stream was deployed over live gRPC channels:

* **Total Telemetry Metrics Processed:** 97 concurrent streaming checks
* **Database Ingestion Success Rate:** 100% (Zero dropped transactions or dropped WAN sockets)
* **Average Edge CPU Processing Overhead:** **`2.48454 microseconds`** (`μs`)

### Performance Significance:
By decoupling public cloud database I/O from the metric ingestion path, the engine sustained an average analytical overhead of **~2.48 μs**, preserving real-time evaluation speeds even while piping transaction frames over the open internet to a remote database cluster.