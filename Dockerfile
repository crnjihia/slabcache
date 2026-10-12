# syntax=docker/dockerfile:1

## Builder stage
FROM ubuntu:24.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    cmake \
    ninja-build \
    g++ \
    libjemalloc-dev \
    libtcmalloc-minimal4 \
    libgoogle-perftools-dev \
    libbenchmark-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

# Configure and build release binary
RUN cmake --preset release && \
    cmake --build --preset release -j

## Runtime stage
FROM ubuntu:24.04

WORKDIR /app

# Copy the server binary from builder
COPY --from=builder /src/build/release/slabcache_server .

EXPOSE 6380

ENTRYPOINT ["./slabcache_server"]
