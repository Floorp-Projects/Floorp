# %ARG DOCKER_IMAGE_PARENT
FROM $DOCKER_IMAGE_PARENT
MAINTAINER Nick Alexander <nalexander@mozilla.com>

VOLUME /builds/worker/checkouts
VOLUME /builds/worker/workspace
VOLUME /builds/worker/tooltool-cache

# rsync is required for l10n single locale repacks.  less, screen, and
# vim, help debugging interactive tasks in Task Cluster.
# git and openssh-client are used to upload GeckoView javadoc to Github.
RUN apt-get update && \
    apt-get install \
      autoconf2.13 \
      build-essential \
      base-files \
      ccache \
      cmake \
      curl \
      file \
      gnupg \
      less \
      openssh-client \
      procps \
      python-cryptography \
      python-dev \
      python-pip \
      python-setuptools \
      python-virtualenv \
      rsync \
      screen \
      sudo \
      tar \
      unzip \
      uuid \
      vim \
      wget \
      yasm \
      zip

# Add wrapper scripts for xvfb allowing tasks to easily retry starting up xvfb.
# %include taskcluster/docker/recipes/xvfb.sh
COPY topsrcdir/taskcluster/docker/recipes/xvfb.sh /builds/worker/scripts/xvfb.sh

# Reset user/workdir from parent image so we can install software.
WORKDIR /
USER root

# Install Sonatype Nexus.  Cribbed directly from
# https://github.com/sonatype/docker-nexus/blob/fffd2c61b2368292040910c055cf690c8e76a272/oss/Dockerfile.

# Observe missing --no-install-recommends, in order to install glib2.0/gconf/etc.
RUN apt-get install \
      libgconf2-4 \
      libgnome2-0 \
      openjdk-8-jdk-headless

ENV NEXUS_VERSION=2.12.0-01 \
    NEXUS_SHA1SUM=1a9aaad8414baffe0a2fd46eed1f41b85f4049e6 \
    NEXUS_WORK=/builds/worker/workspace/nexus

RUN mkdir -p /opt/sonatype/nexus

WORKDIR /tmp
RUN curl --fail --silent --location --retry 3 \
    https://download.sonatype.com/nexus/oss/nexus-${NEXUS_VERSION}-bundle.tar.gz \
    -o /tmp/nexus-${NEXUS_VERSION}-bundle.tar.gz

# Observe the two spaces below.  Seriously.
RUN echo "${NEXUS_SHA1SUM}  nexus-${NEXUS_VERSION}-bundle.tar.gz" > nexus-${NEXUS_VERSION}-bundle.tar.gz.sha1
RUN sha1sum --check nexus-${NEXUS_VERSION}-bundle.tar.gz.sha1

RUN tar zxf nexus-${NEXUS_VERSION}-bundle.tar.gz \
  && mv /tmp/nexus-${NEXUS_VERSION}/* /opt/sonatype/nexus/ \
  && rm -rf /tmp/nexus-${NEXUS_VERSION} \
  && rm -rf /tmp/nexus-${NEXUS_VERSION}-bundle.tar.gz

# So that we don't have to RUN_AS_USER=root.
RUN chown -R worker:worker /opt/sonatype

# Back to the workdir, matching desktop-build.
WORKDIR /builds/worker
