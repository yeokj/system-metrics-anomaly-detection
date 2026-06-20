# Multi-stage build: Compilation environment
FROM ubuntu:24.04 AS builder

LABEL org.opencontainers.image.source="https://github.com/yeokj/system-metrics-anomaly-detection"

# Install development tools and dependency headers
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    libgrpc++-dev \
    protobuf-compiler-grpc \
    libprotobuf-dev \
    protobuf-compiler \
    libpqxx-dev \
    libpq-dev && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Ingest source tree
COPY . .

# Build release binaries via CMake
RUN rm -rf build && mkdir build && \
    cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    make -j$(nproc)

# Multi-stage build: Minimal runtime environment
FROM ubuntu:24.04

WORKDIR /app

# Install dynamic runtime shared libraries
RUN apt-get update && apt-get install -y --no-install-recommends \
    libgrpc++1.51t64 \
    libprotobuf32t64 \
    libpq5 \
    libpqxx-7.8t64 && \
    rm -rf /var/lib/apt/lists/*

# Transfer compiled executable artifacts from builder stage
COPY --from=builder /app/build/TelemetryGenerator /app/bin/TelemetryGenerator
COPY --from=builder /app/build/InferenceServer /app/bin/InferenceServer

# Map serialized ML pipeline parameters to expected execution path
COPY --from=builder /app/python/data/model_params.json /app/python/data/model_params.json

EXPOSE 50051

CMD ["/app/bin/InferenceServer"]