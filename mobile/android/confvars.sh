# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

MOZ_APP_BASENAME=Fennec
MOZ_APP_VENDOR=Mozilla

MOZ_APP_VERSION=17.0a1
MOZ_APP_UA_NAME=Firefox

MOZ_BRANDING_DIRECTORY=mobile/android/branding/unofficial
MOZ_OFFICIAL_BRANDING_DIRECTORY=mobile/android/branding/official
# MOZ_APP_DISPLAYNAME is set by branding/configure.sh

MOZ_SAFE_BROWSING=
MOZ_SERVICES_SYNC=

MOZ_DISABLE_DOMCRYPTO=1

# Enable getUserMedia
MOZ_MEDIA_NAVIGATOR=1

if test "$LIBXUL_SDK"; then
MOZ_XULRUNNER=1
else
MOZ_XULRUNNER=
MOZ_PLACES=1
fi

MOZ_CAPTURE=1
MOZ_RAW=1
MOZ_ANDROID_HISTORY=1

# Needed for building our components as part of libxul
MOZ_APP_COMPONENT_LIBS="browsercomps"
MOZ_APP_COMPONENT_INCLUDE=nsBrowserComponents.h

# use custom widget for html:select
MOZ_USE_NATIVE_POPUP_WINDOWS=1

# dispatch only touch events (no mouse events)
MOZ_ONLY_TOUCH_EVENTS=1

MOZ_APP_ID={aa3c5121-dab2-40e2-81ca-7ea25febc110}

MOZ_JAVA_COMPOSITOR=1
MOZ_EXTENSION_MANAGER=1
MOZ_APP_STATIC_INI=1

