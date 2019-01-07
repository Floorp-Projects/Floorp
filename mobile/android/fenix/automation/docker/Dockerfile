# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

FROM openjdk:8-alpine

MAINTAINER Colin Lee "colinlee@mozilla.com"

#----------------------------------------------------------------------------------------------------------------------
#-- Configuration -----------------------------------------------------------------------------------------------------
#----------------------------------------------------------------------------------------------------------------------

ENV GLIBC_VERSION "2.27-r0"
ENV ANDROID_BUILD_TOOLS "27.0.3"
ENV ANDROID_SDK_VERSION "3859397"
ENV ANDROID_PLATFORM_VERSION "27"
ENV PROJECT_REPOSITORY "https://github.com/mozilla-mobile/fenix.git"

#----------------------------------------------------------------------------------------------------------------------
#-- System ------------------------------------------------------------------------------------------------------------
#----------------------------------------------------------------------------------------------------------------------

RUN apk add --no-cache --virtual=.build-dependencies \
	bash \
	ca-certificates \
	curl \
	git \
	python \
	py-pip \
	unzip \
	wget

RUN pip install --upgrade pip

RUN pip install \
	taskcluster

RUN wget https://raw.githubusercontent.com/sgerrand/alpine-pkg-glibc/master/sgerrand.rsa.pub -O /etc/apk/keys/sgerrand.rsa.pub \
	&& wget https://github.com/sgerrand/alpine-pkg-glibc/releases/download/${GLIBC_VERSION}/glibc-${GLIBC_VERSION}.apk -O /tmp/glibc.apk \
	&& wget https://github.com/sgerrand/alpine-pkg-glibc/releases/download/${GLIBC_VERSION}/glibc-bin-${GLIBC_VERSION}.apk -O /tmp/glibc-bin.apk \
	&& apk add --no-cache /tmp/glibc.apk /tmp/glibc-bin.apk \
	&& rm -rf /tmp/* \
	&& rm -rf /var/cache/apk/*

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
	&& rm sdk.zip

RUN mkdir -p /build/android-sdk/.android/
RUN touch /build/android-sdk/.android/repositories.cfg

RUN yes | sdkmanager --licenses

RUN sdkmanager --verbose "platform-tools" \
    "platforms;android-${ANDROID_PLATFORM_VERSION}" \
    "build-tools;${ANDROID_BUILD_TOOLS}" \
    "extras;android;m2repository" \
	"extras;google;m2repository"

#----------------------------------------------------------------------------------------------------------------------
#-- Project -----------------------------------------------------------------------------------------------------------
#----------------------------------------------------------------------------------------------------------------------

RUN git clone $PROJECT_REPOSITORY

WORKDIR /build/fenix

RUN ./gradlew --no-daemon assemble test lint detektCheck ktlint

#----------------------------------------------------------------------------------------------------------------------
#-- Addendum ----------------------------------------------------------------------------------------------------------
#----------------------------------------------------------------------------------------------------------------------

# Alphine Linux creates a bash profile that overrides our PATH again. Let's fix that.
RUN echo "export PATH=$PATH" >> /etc/profile
