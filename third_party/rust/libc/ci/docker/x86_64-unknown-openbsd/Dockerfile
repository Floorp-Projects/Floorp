FROM ubuntu:16.04

RUN apt-get update
RUN apt-get install -y --no-install-recommends \
  gcc libc6-dev qemu qemu-kvm curl ca-certificates kmod cpu-checker
ENV PATH=$PATH:/rust/bin \
    QEMU=openbsd.qcow2
