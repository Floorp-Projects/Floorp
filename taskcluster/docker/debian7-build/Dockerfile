# %ARG DOCKER_IMAGE_PARENT
FROM $DOCKER_IMAGE_PARENT
MAINTAINER Mike Hommey <mhommey@mozilla.com>

VOLUME /builds/worker/checkouts
VOLUME /builds/worker/workspace
VOLUME /builds/worker/tooltool-cache

ENV XZ_OPT=-T0

# %ARG DOCKER_IMAGE_PACKAGES
RUN /usr/local/sbin/setup_packages.sh $DOCKER_IMAGE_PACKAGES

# %ARG ARCH
RUN dpkg --add-architecture $ARCH

# Ideally, we wouldn't need gcc-multilib and the extra linux-libc-dev,
# but the latter is required to make the former installable, and the former
# because of bug 1409276.
RUN apt-get update && \
    apt-get install \
      autoconf2.13 \
      automake \
      bzip2 \
      curl \
      file \
      gawk \
      gcc-multilib \
      gnupg \
      p7zip-full \
      procps \
      python-pip \
      python-setuptools \
      python-virtualenv \
      rsync \
      screen \
      tar \
      unzip \
      uuid \
      valgrind \
      wget \
      x11-utils \
      xvfb \
      yasm/wheezy-backports \
      zip \
      linux-libc-dev/wheezy-backports \
      linux-libc-dev:$ARCH/wheezy-backports \
      pkg-config:$ARCH \
      libdbus-glib-1-dev:$ARCH \
      libfontconfig1-dev:$ARCH \
      libfreetype6-dev:$ARCH \
      libgconf2-dev:$ARCH \
      libgtk-3-dev:$ARCH \
      libgtk2.0-dev:$ARCH \
      libpango1.0-dev:$ARCH \
      libpulse-dev:$ARCH \
      libx11-xcb-dev:$ARCH \
      libxt-dev:$ARCH

# Add wrapper scripts for xvfb allowing tasks to easily retry starting up xvfb
# %include taskcluster/docker/recipes/xvfb.sh
COPY topsrcdir/taskcluster/docker/recipes/xvfb.sh /builds/worker/scripts/xvfb.sh
