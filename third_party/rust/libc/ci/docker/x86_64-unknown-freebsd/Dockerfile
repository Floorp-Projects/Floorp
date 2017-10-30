FROM alexcrichton/port-prebuilt-freebsd:2017-09-16

RUN apt-get update
RUN apt-get install -y --no-install-recommends \
  qemu genext2fs
RUN apt-get install -y curl ca-certificates gcc

ENTRYPOINT ["sh"]

ENV PATH=$PATH:/rust/bin \
    QEMU=2016-11-06/freebsd.qcow2.gz \
    CAN_CROSS=1 \
    CARGO_TARGET_X86_64_UNKNOWN_FREEBSD_LINKER=x86_64-unknown-freebsd10-gcc
