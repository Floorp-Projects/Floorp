# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

FROM ubuntu:18.04

MAINTAINER Sebastian Kaspari "skaspari@mozilla.com"

#----------------------------------------------------------------------------------------------------------------------
#-- Configuration -----------------------------------------------------------------------------------------------------
#----------------------------------------------------------------------------------------------------------------------

ENV ANDROID_BUILD_TOOLS="28.0.3" \
  ANDROID_HOME=/build/android-sdk \
  ANDROID_SDK_HOME=/build/android-sdk \
  ANDROID_SDK_VERSION="3859397" \
  ANDROID_PLATFORM_VERSION="28" \
  GRADLE_OPTS=-Xmx4096m -Dorg.gradle.daemon=false \
  LANG=en_US.UTF-8 \
  PROJECT_REPOSITORY="https://github.com/mozilla-mobile/android-components.git" \
  TERM=dumb

ENV PATH=${PATH}:${ANDROID_SDK_HOME}/tools:${ANDROID_SDK_HOME}/tools/bin:${ANDROID_SDK_HOME}/platform-tools:/opt/tools:${ANDROID_SDK_HOME}/build-tools/${ANDROID_BUILD_TOOLS}

#----------------------------------------------------------------------------------------------------------------------
#-- System ------------------------------------------------------------------------------------------------------------
#----------------------------------------------------------------------------------------------------------------------

RUN apt-get update -qq \
    # We need to install tzdata before all of the other packages. Otherwise it will show an interactive dialog that
    # we cannot navigate while building the Docker image.
    && apt-get install -y tzdata \
    # python is still needed to fetch taskcluster secrets, for instance.
    && apt-get install -y curl \
                          expect \
                          git \
                          locales \
                          openjdk-8-jdk \
                          python \
                          python-pip \
                          unzip \
    && apt-get clean

RUN locale-gen en_US.UTF-8

#----------------------------------------------------------------------------------------------------------------------
#-- Android -----------------------------------------------------------------------------------------------------------
#----------------------------------------------------------------------------------------------------------------------

RUN mkdir -p /build/android-sdk
WORKDIR /build

COPY requirements.txt requirements.txt
RUN pip --no-cache-dir install --require-hashes --requirement requirements.txt

RUN curl -L https://dl.google.com/android/repository/sdk-tools-linux-${ANDROID_SDK_VERSION}.zip > sdk.zip \
    && unzip sdk.zip -d ${ANDROID_SDK_HOME} \
    && rm sdk.zip \
    && mkdir -p /build/android-sdk/.android/ \
    && touch /build/android-sdk/.android/repositories.cfg \
    && yes | sdkmanager --licenses

#----------------------------------------------------------------------------------------------------------------------
#-- Project -----------------------------------------------------------------------------------------------------------
#----------------------------------------------------------------------------------------------------------------------

RUN git clone --depth=1 $PROJECT_REPOSITORY

WORKDIR /build/android-components

RUN ./gradlew clean \
    && ./gradlew dependencies \
    && ./gradlew androidDependencies \
    && ./gradlew --stop \
    && ./gradlew --no-daemon assemble \
    && ./gradlew --no-daemon -Pcoverage test \
    && ./gradlew --no-daemon detekt \
    && ./gradlew --no-daemon ktlint \
    && ./gradlew --no-daemon docs \
    && ./gradlew clean
