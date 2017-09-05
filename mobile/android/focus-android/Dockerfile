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
