# %ARG DOCKER_IMAGE_PARENT
FROM $DOCKER_IMAGE_PARENT
LABEL maintainer="Andi-Bogdan Postelnicu <andi@mozilla.com>"

VOLUME /builds/worker/checkouts
VOLUME /builds/worker/workspace
VOLUME /builds/worker/tooltool-cache

ENV XZ_OPT=-T0

# %ARG TASKCLUSTER_ROOT_URL
# %ARG DOCKER_IMAGE_PACKAGES
RUN /usr/local/sbin/setup_packages.sh $TASKCLUSTER_ROOT_URL $DOCKER_IMAGE_PACKAGES

RUN apt-get update && \
    apt-get install \
      autoconf2.13 \
      automake \
      bison \
      bzip2 \
      cmake \
      flex \
      curl \
      libsqlite3-dev \
      file \
      gawk \
      gcc-multilib \
      gnupg \
      jq \
      libc6-dev \
      libmpfr-dev \
      nasm \
      openjdk-8-jdk-headless \
      pkg-config \
      patch \
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
      yasm \
      zip \
      zlib1g-dev \
      x11-utils \
      xvfb \
      linux-libc-dev \
      libdbus-glib-1-dev \
      libfontconfig1-dev \
      libfreetype6-dev \
      libgconf2-dev \
      libgmp-dev \
      libgtk-3-dev \
      libgtk2.0-dev \
      libpango1.0-dev \
      libpulse-dev \
      libx11-xcb-dev \
      libxt-dev \
      lib32z1 \
      patchelf

# Install opam 2
RUN curl -sL https://github.com/ocaml/opam/releases/download/2.0.3/opam-2.0.3-x86_64-linux > /usr/bin/opam && \
    chmod +x /usr/bin/opam
