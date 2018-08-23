FROM debian:stretch

RUN apt-get update && apt-get install -y --no-install-recommends \
        curl ca-certificates \
        gcc libc6-dev \
        gcc-sparc64-linux-gnu libc6-dev-sparc64-cross \
        qemu-system-sparc64 openbios-sparc seabios ipxe-qemu \
        p7zip-full cpio linux-libc-dev-sparc64-cross linux-headers-4.9.0-3-common

# Put linux/module.h into the right spot as it is not shipped by debian
RUN cp /usr/src/linux-headers-4.9.0-3-common/include/uapi/linux/module.h /usr/sparc64-linux-gnu/include/linux/

COPY linux-sparc64.sh /
RUN bash /linux-sparc64.sh

COPY test-runner-linux /

ENV CARGO_TARGET_SPARC64_UNKNOWN_LINUX_GNU_LINKER=sparc64-linux-gnu-gcc \
    CARGO_TARGET_SPARC64_UNKNOWN_LINUX_GNU_RUNNER="/test-runner-linux sparc64" \
    CC_sparc64_unknown_linux_gnu=sparc64-linux-gnu-gcc \
    PATH=$PATH:/rust/bin
