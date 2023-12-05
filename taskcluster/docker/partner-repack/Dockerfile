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
      zip

COPY known_hosts /etc/ssh/ssh_known_hosts
