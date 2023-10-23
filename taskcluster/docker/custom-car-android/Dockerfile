FROM $DOCKER_IMAGE_PARENT
MAINTAINER kshampur <kshampur@mozilla.com>

VOLUME /builds/worker/checkouts
VOLUME /builds/worker/workspace
VOLUME /builds/worker/tooltool-cache

ENV XZ_OPT=-T0

RUN apt-get update && \
    apt-get install \
      bzip2 \
      curl \
      libbz2-dev \
      libglib2.0-dev \
      libtinfo5 \
      pkg-config \
      tar \
      unzip \
      wget \
      zip \
      lsb-release \
      gperf \
      procps \
      gcc
