# The image is a reproducible development/SDK environment for the library,
# not a logging daemon. It includes the installed headers and native libraries.
FROM debian:bookworm-slim

ARG DEBIAN_FRONTEND=noninteractive
ARG VERSION=dev
LABEL org.opencontainers.image.title="m-mdy-m/log.c SDK" \
      org.opencontainers.image.description="C11 logging library development image with headers, static/shared libraries, and build tools" \
      org.opencontainers.image.source="https://github.com/m-mdy-m/log.c" \
      org.opencontainers.image.licenses="MIT" \
      org.opencontainers.image.version="$VERSION"

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
       bash build-essential ca-certificates cmake git make pkg-config python3 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /opt/m-mdy-m-log
COPY . .
RUN make clean check \
    && make install PREFIX=/usr/local \
    && install -d /usr/share/doc/m-mdy-m-log \
    && cp README.md LICENSE NOTICE CHANGELOG.md /usr/share/doc/m-mdy-m-log/

ENV PKG_CONFIG_PATH="/usr/local/lib/pkgconfig"
WORKDIR /workspace
CMD ["bash"]
