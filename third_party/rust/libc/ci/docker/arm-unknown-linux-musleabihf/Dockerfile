FROM ubuntu:17.10

RUN apt-get update && apt-get install -y --no-install-recommends \
  gcc make libc6-dev git curl ca-certificates \
  gcc-arm-linux-gnueabihf qemu-user

RUN curl https://www.musl-libc.org/releases/musl-1.1.19.tar.gz | tar xzf -
WORKDIR /musl-1.1.19
RUN CC=arm-linux-gnueabihf-gcc \
    CFLAGS="-march=armv6 -marm" \
    ./configure --prefix=/musl-arm --enable-wrapper=yes
RUN make install -j4

# Install linux kernel headers sanitized for use with musl
RUN curl -L  https://github.com/sabotage-linux/kernel-headers/archive/v3.12.6-6.tar.gz | \
    tar xzf - && \
    cd kernel-headers-3.12.6-6 && \
    make ARCH=arm prefix=/musl-arm install -j4 && \
    cd .. && \
    rm -rf kernel-headers-3.12.6-6

ENV PATH=$PATH:/musl-arm/bin:/rust/bin \
    CC_arm_unknown_linux_musleabihf=musl-gcc \
    CARGO_TARGET_ARM_UNKNOWN_LINUX_MUSLEABIHF_LINKER=musl-gcc \
    CARGO_TARGET_ARM_UNKNOWN_LINUX_MUSLEABIHF_RUNNER="qemu-arm -L /musl-arm"
