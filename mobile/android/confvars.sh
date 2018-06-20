# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

MOZ_APP_BASENAME=Fennec
MOZ_APP_VENDOR=Mozilla

MOZ_APP_VERSION=$FIREFOX_VERSION
MOZ_APP_VERSION_DISPLAY=$FIREFOX_VERSION_DISPLAY
MOZ_APP_UA_NAME=Firefox

MOZ_BRANDING_DIRECTORY=mobile/android/branding/unofficial
MOZ_OFFICIAL_BRANDING_DIRECTORY=mobile/android/branding/official
# MOZ_APP_DISPLAYNAME is set by branding/configure.sh

# We support Android SDK version 16 and up by default.
# See the --enable-android-min-sdk and --enable-android-max-sdk arguments in configure.in.
# 
# Warning: Before increasing the with-android-min-sdk value, please note several places in and out
# of tree have to be changed. Otherwise, places like Treeherder or archive.mozilla.org will
# advertise a bad API level. This may confuse people. As an example, please look at bug 1384482.
# If you think you can't handle the whole set of changes, please reach out to the Release
# Engineering team.
MOZ_ANDROID_MIN_SDK_VERSION=16

# There are several entry points into the Firefox application.  These are the names of some of the classes that are
# listed in the Android manifest.  They are specified in here to avoid hard-coding them in source code files.
MOZ_ANDROID_APPLICATION_CLASS=org.mozilla.gecko.GeckoApplication
MOZ_ANDROID_BROWSER_INTENT_CLASS=org.mozilla.gecko.BrowserApp

MOZ_NO_SMART_CARDS=1

MOZ_XULRUNNER=

MOZ_CAPTURE=1
MOZ_RAW=1

# use custom widget for html:select
MOZ_USE_NATIVE_POPUP_WINDOWS=1

MOZ_APP_ID={aa3c5121-dab2-40e2-81ca-7ea25febc110}
