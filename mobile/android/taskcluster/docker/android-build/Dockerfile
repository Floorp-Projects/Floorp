# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

ARG DOCKER_IMAGE_PARENT
FROM $DOCKER_IMAGE_PARENT

VOLUME /builds/worker/checkouts

# Install Java 8 needed for Nexus
RUN apt-get update -qq \
    && apt-get install -y openjdk-8-jdk \
    && apt-get clean

# Install Sonatype Nexus.  Cribbed directly from
# https://github.com/sonatype/docker-nexus/blob/0415e54b6c824d510f883e5a75c4008b936eca62/oss/Dockerfile.

ENV NEXUS_ARCHIVE='nexus-bundle.tar.gz' \
    NEXUS_ROOT='/opt/sonatype/nexus' \
    NEXUS_SHA1SUM=0fcb4f002eec0cbad6b421b90a8fe99959d78e93 \
    NEXUS_VERSION=2.15.1-02 \
    NEXUS_WORK=/builds/worker/workspace/nexus

RUN mkdir -p "$NEXUS_ROOT" \
  && chown -R worker:worker "$NEXUS_ROOT"

USER worker:worker

RUN $CURL --output "$NEXUS_ARCHIVE" "https://download.sonatype.com/nexus/oss/nexus-${NEXUS_VERSION}-bundle.tar.gz" \
    && echo "$NEXUS_SHA1SUM  $NEXUS_ARCHIVE" | sha1sum --check \
    && tar xzvf "$NEXUS_ARCHIVE" --strip-components=1 --directory="$NEXUS_ROOT" \
    && rm "$NEXUS_ARCHIVE"

# run-task expects to run as root
USER root
