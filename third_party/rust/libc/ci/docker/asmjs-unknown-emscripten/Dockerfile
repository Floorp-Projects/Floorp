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
    CARGO_TARGET_ASMJS_UNKNOWN_EMSCRIPTEN_RUNNER=node

COPY emscripten-entry.sh /
ENTRYPOINT ["/emscripten-entry.sh"]
