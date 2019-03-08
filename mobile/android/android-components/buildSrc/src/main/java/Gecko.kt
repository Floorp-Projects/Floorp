/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

internal object GeckoVersions {
    const val nightly_version = "67.0.20190308093915"
    const val beta_version = "66.0.20190128143734"
    const val release_version = "65.0.20190125215035"
}

@Suppress("MaxLineLength")
object Gecko {
    const val geckoview_nightly_arm = "org.mozilla.geckoview:geckoview-nightly-armeabi-v7a:${GeckoVersions.nightly_version}"
    const val geckoview_nightly_x86 = "org.mozilla.geckoview:geckoview-nightly-x86:${GeckoVersions.nightly_version}"
    const val geckoview_nightly_aarch64 = "org.mozilla.geckoview:geckoview-nightly-arm64-v8a:${GeckoVersions.nightly_version}"

    const val geckoview_beta_arm = "org.mozilla.geckoview:geckoview-beta-armeabi-v7a:${GeckoVersions.beta_version}"
    const val geckoview_beta_x86 = "org.mozilla.geckoview:geckoview-beta-x86:${GeckoVersions.beta_version}"
    const val geckoview_beta_aarch64 = "org.mozilla.geckoview:geckoview-beta-arm64-v8a:${GeckoVersions.beta_version}"

    const val geckoview_release_arm = "org.mozilla.geckoview:geckoview-armeabi-v7a:${GeckoVersions.release_version}"
    const val geckoview_release_x86 = "org.mozilla.geckoview:geckoview-x86:${GeckoVersions.release_version}"
    const val geckoview_release_aarch64 = "org.mozilla.geckoview:geckoview-arm64-v8a:${GeckoVersions.release_version}"
}
