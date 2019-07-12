/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

object GeckoVersions {
    const val release_version = "68.0.20190711090008"
}

@Suppress("MaxLineLength")
object Gecko {
    const val geckoview_release_arm = "org.mozilla.geckoview:geckoview-armeabi-v7a:${GeckoVersions.release_version}"
    const val geckoview_release_aarch64 = "org.mozilla.geckoview:geckoview-arm64-v8a:${GeckoVersions.release_version}"
    const val geckoview_release_x86 = "org.mozilla.geckoview:geckoview-x86:${GeckoVersions.release_version}"
    const val geckoview_release_x86_64 = "org.mozilla.geckoview:geckoview-x86_64:${GeckoVersions.release_version}"
}
