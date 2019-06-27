# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Inspired by:
# https://hub.docker.com/r/runmymind/docker-android-sdk/~/dockerfile/

FROM ubuntu:18.04

MAINTAINER Sebastian Kaspari "skaspari@mozilla.com"

# -- Configuration ----------------------------------------------------------------------

ENV GOOGLE_SDK_VERSION 233

# -- System -----------------------------------------------------------------------------

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
                          locales \
                          unzip \
    && apt-get clean

RUN pip install --upgrade pip
RUN pip install 'taskcluster>=4,<5'
RUN pip install arrow
RUN pip install pyyaml

RUN locale-gen en_US.UTF-8

# -- Android SDK ------------------------------------------------------------------------

RUN mkdir -p /opt/android-sdk
WORKDIR /opt

ENV ANDROID_BUILD_TOOLS "28.0.3"
ENV ANDROID_HOME /opt/android-sdk
ENV ANDROID_SDK_HOME /opt/android-sdk
ENV PATH ${PATH}:${ANDROID_SDK_HOME}/tools:${ANDROID_SDK_HOME}/tools/bin:${ANDROID_SDK_HOME}/platform-tools:/opt/tools:${ANDROID_SDK_HOME}/build-tools/${ANDROID_BUILD_TOOLS}

RUN curl -L https://dl.google.com/android/repository/sdk-tools-linux-4333796.zip > sdk.zip \
	&& unzip sdk.zip -d ${ANDROID_SDK_HOME} \
	&& rm sdk.zip

RUN mkdir -p /opt/android-sdk/.android/
RUN touch /opt/android-sdk/.android/repositories.cfg

RUN yes | sdkmanager --licenses

# Checkout source code
RUN git clone https://github.com/mozilla-mobile/fenix.git

# -- Test tools -------------------------------------------------------------------------

RUN mkdir -p /build/test-tools

WORKDIR /build

ENV TEST_TOOLS /build/test-tools
ENV GOOGLE_SDK_DOWNLOAD ./gcloud.tar.gz
ENV PATH ${PATH}:${TEST_TOOLS}:${TEST_TOOLS}/google-cloud-sdk/bin

RUN curl https://dl.google.com/dl/cloudsdk/channels/rapid/downloads/google-cloud-sdk-${GOOGLE_SDK_VERSION}.0.0-linux-x86_64.tar.gz --output ${GOOGLE_SDK_DOWNLOAD} \
    && tar -xvf ${GOOGLE_SDK_DOWNLOAD} -C ${TEST_TOOLS} \
    && rm -f ${GOOGLE_SDK_DOWNLOAD} \
    && ${TEST_TOOLS}/google-cloud-sdk/install.sh --quiet \
    && ${TEST_TOOLS}/google-cloud-sdk/bin/gcloud --quiet components update

RUN URL_FLANK_BIN=$(curl -s "https://api.github.com/repos/TestArmada/flank/releases/latest" | grep "browser_download_url*" | sed -r "s/\"//g" | cut -d ":" -f3) \
    && wget "https:${URL_FLANK_BIN}" -O ${TEST_TOOLS}/flank.jar \
    && chmod +x ${TEST_TOOLS}/flank.jar

# Build project and run gradle tasks once to pull all dependencies
WORKDIR /opt/fenix
RUN ./gradlew -PversionName=0.0 --no-daemon assemble test lint \
    && ./gradlew clean