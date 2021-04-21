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
      jq \
      less \
      openssh-client \
      procps \
      python-cryptography \
      python-dev \
      python-pip \
      python-setuptools \
      python-virtualenv \
      python3-dev \
      rsync \
      screen \
      sudo \
      tar \
      unzip \
      uuid \
      valgrind \
      vim \
      wget \
      zip \
      zstd

# Add wrapper scripts for xvfb allowing tasks to easily retry starting up xvfb.
# %include taskcluster/docker/recipes/xvfb.sh
COPY topsrcdir/taskcluster/docker/recipes/xvfb.sh /builds/worker/scripts/xvfb.sh

# %include taskcluster/docker/recipes/prepare_openjdk.sh
COPY topsrcdir/taskcluster/docker/recipes/prepare_openjdk.sh /tmp/prepare_openjdk.sh
RUN /tmp/prepare_openjdk.sh && rm /tmp/prepare_openjdk.sh

# Observe missing --no-install-recommends, in order to install glib2.0/gconf/etc.
RUN apt-get install \
      libgconf-2-4 \
      libgnome2-0 \
      openjdk-8-jdk-headless

# Back to the workdir, matching desktop-build.
WORKDIR /builds/worker
