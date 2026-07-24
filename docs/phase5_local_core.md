# Phase 5: Containerization & Continuous Deployment

## 1. Architectural Overview
Phase 5 transitions the system from a locally-compiled backend service into a production-grade, containerized infrastructure suite backed by an automated Continuous Integration and Continuous Deployment (CI/CD) pipeline. 

By leveraging Docker multi-stage builds and GitHub Actions, the system isolates compilation tools from the runtime execution footprint, enforces automated build/lint verification on every pull request, and publishes pre-compiled, immutable container images directly to the GitHub Container Registry (`ghcr.io`).
[Local Code / Push]
       │
       ▼
┌─────────────────────────────────────────────────────────────────┐
│ GitHub Actions CI/CD Pipeline                                   │
│  ├── Job 1: python-lint (flake8 validation)                      │
│  ├── Job 2: cpp-build   (CMake & native build verification)   │
│  └── Job 3: publish-docker (Multi-stage Ubuntu 24.04 image build)│
└────────────────────────────────┬────────────────────────────────┘
                                 │
                                 ▼
                     ┌───────────────────────┐
                     │ GitHub Packages       │
                     │ (ghcr.io Container)   │
                     └───────────────────────┘

---

## 2. Technical Implementation Details

### A. Multi-Stage Docker Build (`Dockerfile`)
To guarantee reproducible compilation without inflating the production runtime image size, a multi-stage `Dockerfile` was engineered:
* **Builder Stage (`ubuntu:24.04`)**: Installs full C++ build chains (`build-essential`, `cmake`), gRPC protoc plugins, PostgreSQL C++ interfaces (`libpqxx-dev`), and dependencies. It compiles the `TelemetryGenerator` and `InferenceServer` release binaries directly from source and generates Protobuf headers.
* **Runtime Stage (`ubuntu:24.04`)**: Strips away compiler tools, copying *only* the compiled executable binaries (`/app/bin/InferenceServer`, `/app/bin/TelemetryGenerator`) and serialized machine learning model parameters (`model_params.json`). Installs only dynamic runtime shared libraries (`libgrpc++`, `libprotobuf`, `libpqxx`).
* **Cache Hardening**: Integrates explicit build workspace purging (`rm -rf build && mkdir build`) to eliminate local host build artifact pollution during container context ingestion.

### B. Multi-Container Orchestration (`docker-compose.yml`)
To support full stack local replication, a multi-service orchestration manifest was defined:
* **`metrics_postgres`**: Spoons a dedicated PostgreSQL 15 container initialized with environment credentials and health checks (`pg_isready`).
* **`metrics_inference_server`**: Builds and runs the gRPC C++ server on port `50051`. Implements an explicit health interlock (`depends_on` with `condition: service_healthy`) ensuring database availability prior to server socket initialization.

### C. Continuous Integration & Delivery Pipeline (`.github/workflows/ci.yml`)
An automated GitHub Actions workflow executes on every commit and pull request to `main`:
1. **`python-lint`**: Verifies syntax and style integrity across Python training and preprocessing modules using `flake8`.
2. **`cpp-build`**: Performs parallel compilation of native C++ code on an Ubuntu runner, catching API mismatch errors and broken CMake bindings early.
3. **`publish-docker`**: Depends on `cpp-build`. Authenticates against GitHub Container Registry using `secrets.GITHUB_TOKEN`, builds the multi-stage Docker image, tags it as `latest`, attaches Open Container Initiative (OCI) repository metadata labels, and pushes the immutable image to `ghcr.io`.

---

## 3. Engineering Challenges & Solutions

### Challenge 1: Cross-Distribution Library Package Naming
* **Issue**: Initial containerization attempts using Alpine Linux (`alpine:3.19`) failed during `apk add` due to missing `libpqxx-dev` system packages, which are unmaintained in standard Alpine mirror indices.
* **Solution**: Migrated the container base layer to `ubuntu:24.04`, aligning package management (`apt-get`) directly with the proven system dependencies established in the GitHub Actions runner environment.

### Challenge 2: Container Image Source Linking
* **Issue**: Pushing the container image to `ghcr.io` initially did not link the package icon directly to the primary GitHub repository sidebar.
* **Solution**: Injected Open Containers Initiative (OCI) labels (`LABEL org.opencontainers.image.source="..."`) directly into the `Dockerfile` and configured `docker/metadata-action@v5` in GitHub Actions to bind image tags directly to repository metadata.

---

## 4. Verification & Deployment Milestones

1. **Automated Pipeline Execution**: GitHub Actions workflow (`System Metrics Anomaly Detection CI`) verified with 3/3 passing concurrent jobs (`python-lint`, `cpp-build`, `publish-docker`).
2. **Registry Publication**: Successfully compiled and published production container image `ghcr.io/yeokj/system-metrics-anomaly-detection:latest`.
3. **Zero-Overhead Deployment**: Verified cross-environment image retrieval via `docker pull ghcr.io/yeokj/system-metrics-anomaly-detection:latest` for zero-configuration, instant execution without local compiler setup.