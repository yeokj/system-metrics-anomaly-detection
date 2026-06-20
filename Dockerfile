# Multi-stage build: Compilation environment
FROM alpine:3.19 AS builder

LABEL org.opencontainers.image.source="https://github.com/yeokj/system-metrics-anomaly-detection"

# Install development tools and dependency headers
RUN apk add --no-cache \
    build-base \
    cmake \
    grpc-dev \
    protobuf-dev \
    libpq-dev \
    libpqxx-dev

WORKDIR /app

# Ingest source tree
COPY . .

# Build release binaries via CMake
RUN mkdir build && \
    cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    make -j$(nproc)

# Multi-stage build: Minimal runtime environment
FROM alpine:3.19

WORKDIR /app

# Install dynamic runtime shared libraries
RUN apk add --no-cache \
    libstdc++ \
    libgcc \
    grpc \
    protobuf \
    libpq \
    libpqxx

# Transfer compiled executable artifacts from builder stage
COPY --from=builder /app/build/TelemetryGenerator /app/bin/TelemetryGenerator
COPY --from=builder /app/build/InferenceServer /app/bin/InferenceServer

# Map serialized ML pipeline parameters to expected execution path
COPY --from=builder /app/python/data/model_params.json /app/python/data/model_params.json

EXPOSE 50051

CMD ["/app/bin/InferenceServer"]