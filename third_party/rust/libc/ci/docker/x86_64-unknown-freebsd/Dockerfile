FROM wezm/port-prebuilt-freebsd11@sha256:43553e2265ec702ec72a63a765df333f50b1858b896e69385749e96d8624e9b0

RUN apt-get update
RUN apt-get install -y --no-install-recommends \
  qemu genext2fs xz-utils
RUN apt-get install -y curl ca-certificates gcc

ENTRYPOINT ["sh"]

ENV PATH=$PATH:/rust/bin \
    QEMU=2018-03-15/FreeBSD-11.1-RELEASE-amd64.qcow2.xz \
    CAN_CROSS=1 \
    CARGO_TARGET_X86_64_UNKNOWN_FREEBSD_LINKER=x86_64-unknown-freebsd11-gcc
