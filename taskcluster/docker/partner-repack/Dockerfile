# %ARG DOCKER_IMAGE_PARENT
FROM $DOCKER_IMAGE_PARENT
MAINTAINER Ben Hearsum <bhearsum@mozilla.com>

VOLUME /builds/worker/checkouts
VOLUME /builds/worker/workspace

RUN dpkg --add-architecture amd64

RUN apt-get update && \
    apt-get install \
      bzip2 \
      curl \
      git \
      gzip \
      openssh-client \
      unzip \
      zip && \
    apt-get clean

# Add wrapper scripts for xvfb for repackage.sh, and interactive sessions
# %include taskcluster/docker/recipes/xvfb.sh
COPY topsrcdir/taskcluster/docker/recipes/xvfb.sh /builds/worker/scripts/xvfb.sh

COPY known_hosts /etc/ssh/ssh_known_hosts
