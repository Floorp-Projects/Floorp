# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

FROM ubuntu:18.04

MAINTAINER Sebastian Kaspari "skaspari@mozilla.com"

#----------------------------------------------------------------------------------------------------------------------
#-- Configuration -----------------------------------------------------------------------------------------------------
#----------------------------------------------------------------------------------------------------------------------

ENV ANDROID_BUILD_TOOLS "27.0.3"
ENV ANDROID_SDK_VERSION "3859397"
ENV ANDROID_PLATFORM_VERSION "27"
ENV PROJECT_REPOSITORY "https://github.com/mozilla-mobile/android-components.git"

ENV LANG en_US.UTF-8

# Do not use fancy output on taskcluster
ENV TERM dumb

ENV GRADLE_OPTS -Xmx4096m -Dorg.gradle.daemon=false

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
                          locales \
                          unzip \
    && apt-get clean

RUN pip install --upgrade pip
RUN pip install taskcluster

RUN locale-gen en_US.UTF-8

#----------------------------------------------------------------------------------------------------------------------
#-- Android -----------------------------------------------------------------------------------------------------------
#----------------------------------------------------------------------------------------------------------------------

RUN mkdir -p /build/android-sdk
WORKDIR /build

ENV ANDROID_HOME /build/android-sdk
ENV ANDROID_SDK_HOME /build/android-sdk
ENV PATH ${PATH}:${ANDROID_SDK_HOME}/tools:${ANDROID_SDK_HOME}/tools/bin:${ANDROID_SDK_HOME}/platform-tools:/opt/tools:${ANDROID_SDK_HOME}/build-tools/${ANDROID_BUILD_TOOLS}

RUN curl -L https://dl.google.com/android/repository/sdk-tools-linux-${ANDROID_SDK_VERSION}.zip > sdk.zip \
    && unzip sdk.zip -d ${ANDROID_SDK_HOME} \
    && rm sdk.zip \
    && mkdir -p /build/android-sdk/.android/ \
    && touch /build/android-sdk/.android/repositories.cfg \
    && yes | sdkmanager --licenses \
    && sdkmanager --verbose "platform-tools" \
        "platforms;android-${ANDROID_PLATFORM_VERSION}" \
        "build-tools;${ANDROID_BUILD_TOOLS}" \
        "extras;android;m2repository" \
        "extras;google;m2repository"
#----------------------------------------------------------------------------------------------------------------------
#-- Project -----------------------------------------------------------------------------------------------------------
#----------------------------------------------------------------------------------------------------------------------

RUN git clone $PROJECT_REPOSITORY

WORKDIR /build/android-components

RUN ./gradlew clean \
    && ./gradlew dependencies \
    && ./gradlew androidDependencies \
    && ./gradlew --stop \
    && ./gradlew --no-daemon assemble \
    && ./gradlew --no-daemon test \
    && ./gradlew --no-daemon detektCheck \
    && ./gradlew --no-daemon ktlint \
    && ./gradlew --no-daemon docs
