FROM alpine:3.19 AS builder

RUN apk add --no-cache \
    build-base \
    cmake \
    grpc-dev \
    protobuf-dev \
    libpq-dev \
    libpqxx-dev