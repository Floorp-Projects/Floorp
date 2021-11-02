FROM $DOCKER_IMAGE_PARENT
MAINTAINER Agi Sferro <asferro@mozilla.com>

VOLUME /builds/worker/.cache
VOLUME /builds/worker/checkouts
VOLUME /builds/worker/tooltool-cache
VOLUME /builds/worker/workspace

# Workaround for missing folder in the Ubuntu image, see
# https://github.com/debuerreotype/docker-debian-artifacts/issues/24 and
# related.
RUN mkdir -p /usr/share/man/man1

RUN apt-get update && \
    apt-get install \
      openjdk-11-jdk-headless

CMD ["/bin/bash", "--login"]
