# %ARG DOCKER_IMAGE_PARENT
FROM $DOCKER_IMAGE_PARENT
MAINTAINER Robert Bartlensky <rbartlensky@mozilla.com>

VOLUME /builds/worker/checkouts
VOLUME /builds/worker/workspace
VOLUME /builds/worker/tooltool-cache

ENV XZ_OPT=-T0

RUN apt-get update && \
    apt-get install \
      autoconf \
      bison \
      bzip2 \
      flex \
      curl \
      git \
      opam \
      libsqlite3-dev \
      autoconf \
      automake \
      cmake \
      libc6-dev \
      openjdk-8-jdk-headless \
      pkg-config \
      patch \
      tar \
      unzip \
      zlib1g-dev
