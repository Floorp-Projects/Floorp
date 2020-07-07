FROM $DOCKER_IMAGE_PARENT
MAINTAINER Rob Lemley <rob@thunderbird.net>
# Used by Thunderbird to build third party libraries for OTR messaging.

VOLUME /builds/worker/checkouts
VOLUME /builds/worker/workspace
VOLUME /builds/worker/tooltool-cache

RUN apt-get update && \
    apt-get dist-upgrade && \
    apt-get install \
      autoconf \
      automake \
      binutils-mingw-w64 \
      gcc-mingw-w64 \
      gcc-mingw-w64-i686 \
      gcc-mingw-w64-x86-64 \
      libtool \
      mingw-w64-tools
