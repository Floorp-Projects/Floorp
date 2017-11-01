# link fails on 17.10
FROM ubuntu:17.04

RUN apt-get update && apt-get install -y --no-install-recommends \
        gcc libc6-dev qemu-user ca-certificates \
        gcc-sparc64-linux-gnu libc6-dev-sparc64-cross

ENV CARGO_TARGET_SPARC64_UNKNOWN_LINUX_GNU_LINKER=sparc64-linux-gnu-gcc \
    # TODO: in theory we should execute this, but qemu segfaults immediately
    #       see https://github.com/rust-lang/libc/issues/822
    # CARGO_TARGET_SPARC64_UNKNOWN_LINUX_GNU_RUNNER="qemu-sparc64 -L /usr/sparc64-linux-gnu" \
    CARGO_TARGET_SPARC64_UNKNOWN_LINUX_GNU_RUNNER=true \
    CC_sparc64_unknown_linux_gnu=sparc64-linux-gnu-gcc \
    PATH=$PATH:/rust/bin
