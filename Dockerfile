FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends git build-essential cmake python3 python3-pip python3-venv curl wget ca-certificates nodejs npm sudo && rm -rf /var/lib/apt/lists/*
RUN useradd -m -s /bin/bash dev && echo "dev ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/dev
USER dev
WORKDIR /workspaces/sense-pong