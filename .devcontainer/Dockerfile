FROM mcr.microsoft.com/devcontainers/base:ubuntu-24.04

ARG ARDUINO_CLI_VERSION=1.2.2

RUN apt-get update && apt-get install -y --no-install-recommends \
    curl \
    ca-certificates \
    python3 \
    python3-pip \
    python3-venv \
    && rm -rf /var/lib/apt/lists/*

RUN curl -fsSL "https://downloads.arduino.cc/arduino-cli/arduino-cli_${ARDUINO_CLI_VERSION}_Linux_64bit.tar.gz" \
    | tar -xz -C /usr/local/bin arduino-cli

USER vscode
WORKDIR /workspaces/spotify-album-display
