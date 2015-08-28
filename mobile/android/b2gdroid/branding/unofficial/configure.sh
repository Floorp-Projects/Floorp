# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

ANDROID_PACKAGE_NAME=org.mozilla.b2gdroid_`echo $USER | sed 's/-/_/g'`
MOZ_APP_DISPLAYNAME="B2GDroid `echo $USER | sed 's/-/_/g'`"
MOZ_UPDATER=1
MOZ_ANDROID_ANR_REPORTER=
