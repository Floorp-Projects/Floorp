FROM ubuntu:16.04

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    ca-certificates \
    curl \
    gcc \
    git \
    libc6-dev \
    python \
    xz-utils

COPY emscripten.sh /
RUN bash /emscripten.sh

ENV PATH=$PATH:/rust/bin \
    CARGO_TARGET_WASM32_UNKNOWN_EMSCRIPTEN_RUNNER=node-wrapper.sh

COPY emscripten-entry.sh /
COPY docker/wasm32-unknown-emscripten/node-wrapper.sh /usr/local/bin/node-wrapper.sh
ENTRYPOINT ["/emscripten-entry.sh"]
