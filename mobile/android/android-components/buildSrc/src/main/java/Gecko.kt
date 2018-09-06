/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

private object GeckoVersions {
    const val nightly_version = "64.0.20180905100117"
}

object Gecko {
    const val geckoview_nightly_arm = "org.mozilla.geckoview:geckoview-nightly-armeabi-v7a:${GeckoVersions.nightly_version}"
    const val geckoview_nightly_x86 = "org.mozilla.geckoview:geckoview-nightly-x86:${GeckoVersions.nightly_version}"
    const val geckoview_nightly_aarch64 = "org.mozilla.geckoview:geckoview-nightly-arm64-v8a:${GeckoVersions.nightly_version}"
}
