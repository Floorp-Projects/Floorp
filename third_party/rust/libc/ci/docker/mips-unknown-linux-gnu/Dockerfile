FROM ubuntu:17.10

RUN apt-get update && apt-get install -y --no-install-recommends \
        gcc libc6-dev qemu-user ca-certificates \
        gcc-mips-linux-gnu libc6-dev-mips-cross \
        qemu-system-mips

ENV CARGO_TARGET_MIPS_UNKNOWN_LINUX_GNU_LINKER=mips-linux-gnu-gcc \
    CARGO_TARGET_MIPS_UNKNOWN_LINUX_GNU_RUNNER="qemu-mips -L /usr/mips-linux-gnu" \
    PATH=$PATH:/rust/bin
