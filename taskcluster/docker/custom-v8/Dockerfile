# %ARG DOCKER_IMAGE_PARENT
FROM $DOCKER_IMAGE_PARENT
MAINTAINER Joel Maher <jmaher@mozilla.com>

VOLUME /builds/worker/checkouts
VOLUME /builds/worker/workspace
VOLUME /builds/worker/tooltool-cache

ENV XZ_OPT=-T0

# %ARG DOCKER_IMAGE_PACKAGES
RUN /usr/local/sbin/setup_packages.sh $DOCKER_IMAGE_PACKAGES

RUN apt-get update && \
    apt-get install \
      bzip2 \
      curl \
      libbz2-dev \
      lsb-core \
      sudo \
      tar \
      unzip \
      wget \
      zip

RUN adduser worker sudo
RUN echo '%sudo ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers
