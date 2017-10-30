FROM ubuntu:17.10

RUN apt-get update && apt-get install -y --no-install-recommends \
        gcc libc6-dev qemu-user ca-certificates \
        gcc-s390x-linux-gnu libc6-dev-s390x-cross

ENV CARGO_TARGET_S390X_UNKNOWN_LINUX_GNU_LINKER=s390x-linux-gnu-gcc \
    # TODO: in theory we should execute this, but qemu segfaults immediately :(
    # CARGO_TARGET_S390X_UNKNOWN_LINUX_GNU_RUNNER="qemu-s390x -L /usr/s390x-linux-gnu" \
    CARGO_TARGET_S390X_UNKNOWN_LINUX_GNU_RUNNER=true \
    CC_s390x_unknown_linux_gnu=s390x-linux-gnu-gcc \
    PATH=$PATH:/rust/bin
