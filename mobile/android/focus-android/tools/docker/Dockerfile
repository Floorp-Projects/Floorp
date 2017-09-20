# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

FROM runmymind/docker-android-sdk

# -- Android SDK ------------------------------------------------------------------------

# Android 8.0 / SDK 26
RUN echo y | android update sdk --no-ui --all --filter android-26 | grep 'package installed'

# Build tools 26.0.1
RUN echo y | android update sdk --no-ui --all --filter build-tools-25.0.3 | grep 'package installed'


# -- Project setup ----------------------------------------------------------------------

# Checkout source code
RUN git clone https://github.com/mozilla-mobile/focus-android.git

# Build project once to pull all dependencies
WORKDIR /focus-android
RUN ./gradlew assemble
