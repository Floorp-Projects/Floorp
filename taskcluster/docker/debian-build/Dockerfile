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
# libdbus-glib-1-2 and libgtk-3-0 are needed to run xpcshell during the build.
# lib32atomic1, lib32stdc++6 and lib32z1 are needed to run some 32-bits
# spidermonkey tests.
RUN apt-get update && \
    apt-get dist-upgrade && \
    apt-get install \
      binutils \
      bzip2 \
      curl \
      file \
      gawk \
      gnupg \
      jq \
      lib32atomic1 \
      'lib32gcc(1|-s1)$' \
      lib32stdc++6 \
      lib32z1 \
      libc6-i386 \
      libdbus-glib-1-2 \
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
      xvfb \
      zip \
      pkg-config

# Add wrapper scripts for xvfb allowing tasks to easily retry starting up xvfb
# %include taskcluster/docker/recipes/xvfb.sh
COPY topsrcdir/taskcluster/docker/recipes/xvfb.sh /builds/worker/scripts/xvfb.sh
