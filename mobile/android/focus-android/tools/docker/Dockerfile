# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Inspired by:
# https://hub.docker.com/r/runmymind/docker-android-sdk/~/dockerfile/

FROM ubuntu:17.04

MAINTAINER Sebastian Kaspari "skaspari@mozilla.com"

# -- System -----------------------------------------------------------------------------

RUN apt-get update -qq

RUN apt-get install -y openjdk-8-jdk \
					   wget \
					   expect \
					   git \
					   curl \
					   ruby \
					   ruby-dev \
					   ruby-build \
					   python \
					   optipng \
					   imagemagick
RUN gem install fastlane

# -- Android SDK ------------------------------------------------------------------------

RUN cd /opt && wget -q https://dl.google.com/android/android-sdk_r24.4.1-linux.tgz -O android-sdk.tgz
RUN cd /opt && tar -xvzf android-sdk.tgz
RUN cd /opt && rm -f android-sdk.tgz

ENV ANDROID_SDK_HOME /opt/android-sdk-linux
ENV ANDROID_HOME /opt/android-sdk-linux
ENV PATH ${PATH}:${ANDROID_SDK_HOME}/tools:${ANDROID_SDK_HOME}/platform-tools:/opt/tools

# Platform tools
RUN echo y | android update sdk --no-ui --all --filter platform-tools | grep 'package installed'

# Android 8.0 / SDK 26
RUN echo y | android update sdk --no-ui --all --filter android-26 | grep 'package installed'

# Build tools 26.0.1
RUN echo y | android update sdk --no-ui --all --filter build-tools-26.0.1 | grep 'package installed'

# Extras
RUN echo y | android update sdk --no-ui --all --filter extra-android-m2repository | grep 'package installed'
RUN echo y | android update sdk --no-ui --all --filter extra-google-m2repository | grep 'package installed'
RUN echo y | android update sdk --no-ui --all --filter extra-google-google_play_services | grep 'package installed'

# Copy tools from our repository into the docker image
COPY tools /opt/tools

# Accept licenses
COPY licenses ${ANDROID_SDK_HOME}/licenses

# -- Emulator ---------------------------------------------------------------------------

# Android 5.0 / SDK 21
RUN echo y | android update sdk --no-ui --all --filter android-21 | grep 'package installed'

# Emulator image
RUN echo y | android update sdk --no-ui --all --filter sys-img-armeabi-v7a-android-21 | grep 'package installed'

# Create emulator AVD
RUN echo no | android create avd -f -n test -t android-21 --abi default/armeabi-v7a --skin 480x800

# Create fake keymap file (Otherwise the emulator won't start up)
RUN mkdir /opt/android-sdk-linux/tools/keymaps && \
    touch /opt/android-sdk-linux/tools/keymaps/en-us

# -- Update SDK -------------------------------------------------------------------------

# Update SDK
RUN /opt/tools/android-accept-licenses.sh android update sdk --no-ui --obsolete --force

# -- Project setup ----------------------------------------------------------------------

WORKDIR /opt

# Checkout source code
RUN git clone https://github.com/mozilla-mobile/focus-android.git

# Build project once to pull all dependencies
WORKDIR /opt/focus-android
RUN ./gradlew assemble

# -- Cleanup ----------------------------------------------------------------------------

RUN apt-get clean
