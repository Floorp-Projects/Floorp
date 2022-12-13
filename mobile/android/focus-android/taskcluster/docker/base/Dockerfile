# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Inspired by:
# https://hub.docker.com/r/runmymind/docker-android-sdk/~/dockerfile/

FROM ubuntu:18.04

MAINTAINER Release Engineering <releng@mozilla.com>

# Add worker user
RUN mkdir /builds && \
    useradd -d /builds/worker -s /bin/bash -m worker && \
    chown worker:worker /builds/worker && \
    mkdir /builds/worker/artifacts && \
    chown worker:worker /builds/worker/artifacts

WORKDIR /builds/worker/

# -- System ----------------------------------------------------------------------------

ENV CURL='curl --location --retry 5' \
    GRADLE_OPTS='-Xmx4096m -Dorg.gradle.daemon=false' \
    LANG='en_US.UTF-8' \
    TERM='dumb'

RUN apt-get update -qq \
    # We need to install tzdata before all of the other packages. Otherwise it will show an interactive dialog that
    # we cannot navigate while building the Docker image.
    && apt-get install -y tzdata \
    && apt-get install -y openjdk-8-jdk \
                          openjdk-11-jdk \
                          wget \
                          expect \
                          git \
                          curl \
                          python \
                          python-pip \
                          python3 \
                          python3-pip \
                          python3-yaml \
                          locales \
                          unzip \
			  mercurial \
    && apt-get clean


RUN pip3 install --upgrade pip
RUN pip3 install taskcluster

RUN locale-gen en_US.UTF-8

# Do not delete - this is magic code that will enable the run-task worker for Taskgraph
# %include-run-task

ENV SHELL=/bin/bash \
    HOME=/builds/worker \
    PATH="/builds/worker/.local/bin:$PATH"


VOLUME /builds/worker/checkouts
VOLUME /builds/worker/.cache


# run-task expects to run as root
USER root

