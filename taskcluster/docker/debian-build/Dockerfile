FROM $DOCKER_IMAGE_PARENT
MAINTAINER Mike Hommey <mhommey@mozilla.com>

VOLUME /builds/worker/checkouts
VOLUME /builds/worker/workspace
VOLUME /builds/worker/tooltool-cache

ENV XZ_OPT=-T0

ARG TASKCLUSTER_ROOT_URL
ARG DOCKER_IMAGE_PACKAGES
RUN /usr/local/sbin/setup_packages.sh $TASKCLUSTER_ROOT_URL $DOCKER_IMAGE_PACKAGES

# binutils is needed for base toolchain builds. Ideally, it wouldn't be needed.
# libc6-i386 and lib32gcc1 are needed for wine.
# libgtk-3-0 are needed to run xpcshell during the build.
# lib32atomic1, lib32stdc++6 and lib32z1 are needed to run some 32-bits
# cpio is needed by xar (mac repackaging on linux)
# spidermonkey tests. libasound2 is needed to run xpcshell after we introduced
# the dependencies on alsa via Web MIDI.
RUN apt-get update && \
    apt-get dist-upgrade && \
    apt-get install \
      binutils \
      bzip2 \
      curl \
      cpio \
      file \
      gawk \
      gnupg \
      jq \
      lib32atomic1 \
      'lib32gcc(1|-s1)$' \
      lib32stdc++6 \
      lib32z1 \
      libasound2 \
      libc6-i386 \
      libgtk-3-0 \
      libucl1 \
      p7zip-full \
      procps \
      python3-dev \
      rsync \
      screen \
      tar \
      unzip \
      uuid \
      wget \
      x11-utils \
      zip
