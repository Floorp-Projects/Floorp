# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

ARG DOCKER_IMAGE_PARENT
FROM $DOCKER_IMAGE_PARENT

MAINTAINER Johan Lorenzo <jlorenzo+tc@mozilla.com>

VOLUME /builds/worker/checkouts

# Install Sonatype Nexus.  Cribbed directly from
# https://github.com/sonatype/docker-nexus/blob/fffd2c61b2368292040910c055cf690c8e76a272/oss/Dockerfile.

ENV NEXUS_ARCHIVE='nexus-bundle.tar.gz' \
    NEXUS_ROOT='/opt/sonatype/nexus' \
    NEXUS_SHA1SUM=1a9aaad8414baffe0a2fd46eed1f41b85f4049e6 \
    NEXUS_VERSION=2.12.0-01 \
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
