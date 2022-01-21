# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

FROM ubuntu:18.04

# Add worker user
RUN mkdir /builds && \
    useradd -d /builds/worker -s /bin/bash -m worker && \
    chown worker:worker /builds/worker && \
    mkdir /builds/worker/artifacts && \
    chown worker:worker /builds/worker/artifacts

WORKDIR /builds/worker/

#----------------------------------------------------------------------------------------------------------------------
#-- Configuration -----------------------------------------------------------------------------------------------------
#----------------------------------------------------------------------------------------------------------------------

ENV CURL='curl --location --retry 5' \
    GRADLE_OPTS='-Xmx4096m -Dorg.gradle.daemon=false' \
    LANG='en_US.UTF-8' \
    TERM='dumb' \
    DEBIAN_FRONTEND=noninteractive

#----------------------------------------------------------------------------------------------------------------------
#-- System ------------------------------------------------------------------------------------------------------------
#----------------------------------------------------------------------------------------------------------------------

RUN apt-get update -qq \
    && apt-get install -y openjdk-8-jdk \
                          openjdk-11-jdk \
                          wget \
                          expect \
                          git \
                          curl \
                          python \
                          python-pip \
                          python3 \
                          python3-yaml \
                          locales \
                          tzdata \
                          unzip \
			  mercurial \
    && apt-get clean

RUN pip install --upgrade pip
# TODO update to python3 so we can use newer taskcluster
RUN pip install taskcluster==44.2.2

RUN locale-gen "$LANG"

# %include-run-task

ENV SHELL=/bin/bash \
    HOME=/builds/worker \
    PATH="/builds/worker/.local/bin:$PATH"


VOLUME /builds/worker/checkouts
VOLUME /builds/worker/.cache


# run-task expects to run as root
USER root
