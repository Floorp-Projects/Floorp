# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

MOZ_APP_BASENAME=Fennec
MOZ_APP_VENDOR=Mozilla

MOZ_APP_VERSION=32.0.3
MOZ_APP_UA_NAME=Firefox

MOZ_BRANDING_DIRECTORY=mobile/android/branding/unofficial
MOZ_OFFICIAL_BRANDING_DIRECTORY=mobile/android/branding/official
# MOZ_APP_DISPLAYNAME is set by branding/configure.sh

MOZ_SAFE_BROWSING=1

MOZ_DISABLE_CRYPTOLEGACY=1

# Enable getUserMedia
MOZ_MEDIA_NAVIGATOR=1

# Enable NFC permission
MOZ_ANDROID_BEAM=1

if test "$LIBXUL_SDK"; then
MOZ_XULRUNNER=1
else
MOZ_XULRUNNER=
fi

MOZ_CAPTURE=1
MOZ_RAW=1
MOZ_PLACES=
MOZ_SOCIAL=
MOZ_ANDROID_HISTORY=1
MOZ_DISABLE_EXPORT_JS=1

# Needed for building our components as part of libxul
MOZ_APP_COMPONENT_INCLUDE=nsBrowserComponents.h

# use custom widget for html:select
MOZ_USE_NATIVE_POPUP_WINDOWS=1

MOZ_APP_ID={aa3c5121-dab2-40e2-81ca-7ea25febc110}

MOZ_ANDROID_OMTC=1
MOZ_EXTENSION_MANAGER=1
MOZ_APP_STATIC_INI=1

# Enable on-demand decompression
MOZ_ENABLE_SZIP=1

MOZ_FOLD_LIBS=1

# Enable navigator.mozPay
MOZ_PAY=1

# Enable UI for healthreporter
MOZ_SERVICES_HEALTHREPORT=1

# Enable FirefoxAccounts
MOZ_SERVICES_FXACCOUNTS=1

# Wifi-AP/cell tower data reporting is enabled on non-release builds.
if test ! "$RELEASE_BUILD"; then
MOZ_DATA_REPORTING=1
fi

# Enable runtime locale switching.
MOZ_LOCALE_SWITCHER=1

# Enable the "synthetic APKs" implementation of Open Web Apps.
MOZ_ANDROID_SYNTHAPKS=1

# Enable second screen and casting support for external devices.
MOZ_DEVICES=1
