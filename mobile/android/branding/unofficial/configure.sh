# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

ANDROID_PACKAGE_NAME=org.mozilla.fennec_`echo ${USER:-unknown} | sed 's/-/_/g'`
MOZ_APP_DISPLAYNAME="Fennec `echo ${USER:-unknown} | sed 's/-/_/g'`"
MOZ_UPDATER=
