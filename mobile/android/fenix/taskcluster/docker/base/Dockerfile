# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

FROM ubuntu:18.04

MAINTAINER Tom Prince "mozilla@hocat.ca"

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

ENV ANDROID_SDK_VERSION='3859397' \
    ANDROID_SDK_ROOT='/builds/worker/android-sdk-linux' \
    GRADLE_OPTS='-Xmx4096m -Dorg.gradle.daemon=false' \
    LANG='en_US.UTF-8' \
    TERM='dumb' \
    SDK_ZIP_LOCATION="$HOME/sdk-tools-linux.zip"

#----------------------------------------------------------------------------------------------------------------------
#-- System ------------------------------------------------------------------------------------------------------------
#----------------------------------------------------------------------------------------------------------------------

RUN apt-get update -qq \
    # We need to install tzdata before all of the other packages. Otherwise it will show an interactive dialog that
    # we cannot navigate while building the Docker image.
    && apt-get install -y tzdata \
    && apt-get install -y openjdk-8-jdk \
                          wget \
                          expect \
                          git \
                          curl \
                          python \
                          python-pip \
                          python3 \
                          locales \
                          unzip \
			  mercurial \
    && apt-get clean

RUN pip install --upgrade pip
RUN pip install taskcluster

RUN locale-gen "$LANG"

RUN curl -o "$SDK_ZIP_LOCATION" "https://dl.google.com/android/repository/sdk-tools-linux-${ANDROID_SDK_VERSION}.zip" \
    && unzip -d "$ANDROID_SDK_ROOT" "$SDK_ZIP_LOCATION" \
    && rm "$SDK_ZIP_LOCATION" \
    && yes | "${ANDROID_SDK_ROOT}/tools/bin/sdkmanager" --licenses \
    && chown -R worker:worker "$ANDROID_SDK_ROOT"


# %include-run-task

ENV SHELL=/bin/bash \
    HOME=/builds/worker \
    PATH=/builds/worker/.local/bin:$PATH


VOLUME /builds/worker/checkouts
VOLUME /builds/worker/.cache


# run-task expects to run as root
USER root
