# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

ANDROID_PACKAGE_NAME=org.mozilla.fennec_`echo ${USER:-unknown} | sed 's/-/_/g'`
MOZ_APP_DISPLAYNAME="Fennec `echo ${USER:-unknown} | sed 's/-/_/g'`"
MOZ_UPDATER=
MOZ_ANDROID_ANR_REPORTER=
MOZ_ANDROID_GCM_SENDERID=965234145045
MOZ_MMA_GCM_SENDERID=242693410970
