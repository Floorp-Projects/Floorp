FROM ubuntu:17.10
RUN apt-get update && apt-get install -y --no-install-recommends \
  gcc \
  ca-certificates \
  libc6-dev \
  libc6-armel-cross \
  libc6-dev-armel-cross \
  binutils-arm-linux-gnueabi \
  gcc-arm-linux-gnueabi \
  qemu-user \
  make \
  file
ENV CARGO_TARGET_ARM_UNKNOWN_LINUX_GNUEABI_LINKER=arm-linux-gnueabi-gcc \
    CARGO_TARGET_ARM_UNKNOWN_LINUX_GNUEABI_RUNNER="qemu-arm -L /usr/arm-linux-gnueabi" \
    OBJDUMP=arm-linux-gnueabi-objdump
