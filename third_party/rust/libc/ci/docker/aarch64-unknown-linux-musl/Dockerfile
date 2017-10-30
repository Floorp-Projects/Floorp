FROM ubuntu:17.10

RUN apt-get update && apt-get install -y --no-install-recommends \
  gcc make libc6-dev git curl ca-certificates \
  gcc-aarch64-linux-gnu qemu-user
RUN curl https://www.musl-libc.org/releases/musl-1.1.16.tar.gz | \
    tar xzf - && \
    cd musl-1.1.16 && \
    CC=aarch64-linux-gnu-gcc \
    ./configure --prefix=/musl-aarch64 --enable-wrapper=yes && \
    make install -j4 && \
    cd .. && \
    rm -rf musl-1.1.16 && \
# Install linux kernel headers sanitized for use with musl
    curl -L  https://github.com/sabotage-linux/kernel-headers/archive/v3.12.6-5.tar.gz | \
    tar xzf - && \
    cd kernel-headers-3.12.6-5 && \
    make ARCH=arm64 prefix=/musl-aarch64 install -j4 && \
    cd .. && \
    rm -rf kernel-headers-3.12.6-5

# FIXME: shouldn't need the `-lgcc` here, shouldn't that be in libstd?
ENV PATH=$PATH:/musl-aarch64/bin:/rust/bin \
    CC_aarch64_unknown_linux_musl=musl-gcc \
    RUSTFLAGS='-Clink-args=-lgcc' \
    CARGO_TARGET_AARCH64_UNKNOWN_LINUX_MUSL_LINKER=musl-gcc \
    CARGO_TARGET_AARCH64_UNKNOWN_LINUX_MUSL_RUNNER="qemu-aarch64 -L /musl-aarch64"
