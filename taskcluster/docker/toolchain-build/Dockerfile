# %ARG DOCKER_IMAGE_PARENT
FROM $DOCKER_IMAGE_PARENT
MAINTAINER Mike Hommey <mhommey@mozilla.com>

VOLUME /builds/worker/checkouts
VOLUME /builds/worker/workspace
VOLUME /builds/worker/tooltool-cache

ENV XZ_OPT=-T0

# %ARG DOCKER_IMAGE_PACKAGES
# %ARG TASKCLUSTER_ROOT_URL
RUN /usr/local/sbin/setup_packages.sh $TASKCLUSTER_ROOT_URL $DOCKER_IMAGE_PACKAGES

RUN apt-get update && \
    apt-get install \
      autoconf \
      autoconf2.13 \
      automake \
      bison \
      build-essential \
      curl \
      cmake \
      flex \
      gawk \
      gcc-multilib \
      gnupg \
      jq \
      libbz2-dev \
      libcurl4-openssl-dev \
      libssl-dev \
      libxml2-dev \
      libtool \
      libucl-dev \
      ninja-build \
      p7zip-full \
      procps \
      python-pip \
      python-setuptools \
      python-virtualenv \
      python3-distutils-extra \
      subversion \
      tar \
      unzip \
      uuid \
      uuid-dev \
      wget \
      zip \
      zlib1g-dev
