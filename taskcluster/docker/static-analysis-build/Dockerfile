# %ARG DOCKER_IMAGE_PARENT
FROM $DOCKER_IMAGE_PARENT
MAINTAINER Robert Bartlensky <rbartlensky@mozilla.com>

VOLUME /builds/worker/checkouts
VOLUME /builds/worker/workspace
VOLUME /builds/worker/tooltool-cache

ENV XZ_OPT=-T0

RUN apt-get update && \
    apt-get install \
      autoconf2.13 \
      automake \
      bison \
      bzip2 \
      cmake \
      flex \
      curl \
      opam \
      libsqlite3-dev \
      file \
      gawk \
      gcc-multilib \
      gnupg \
      libc6-dev \
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
      libgtk-3-dev \
      libgtk2.0-dev \
      libpango1.0-dev \
      libpulse-dev \
      libx11-xcb-dev \
      libxt-dev \
      lib32z1
