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
      python3-dev \
      rsync \
      screen \
      sudo \
      tar \
      unzip \
      uuid \
      valgrind \
      vim \
      x11-utils \
      xvfb \
      wget \
      zip \
      zstd

# Add wrapper scripts for xvfb allowing tasks to easily retry starting up xvfb.
# %include taskcluster/docker/recipes/xvfb.sh
COPY topsrcdir/taskcluster/docker/recipes/xvfb.sh /builds/worker/scripts/xvfb.sh

# Back to the workdir, matching desktop-build.
WORKDIR /builds/worker
