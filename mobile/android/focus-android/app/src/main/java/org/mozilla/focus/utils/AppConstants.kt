/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.utils

import org.mozilla.focus.BuildConfig

object AppConstants {
    private const val BUILD_TYPE_RELEASE = "release"
    private const val BUILD_TYPE_BETA = "beta"
    private const val BUILD_TYPE_DEBUG = "debug"
    private const val BUILD_TYPE_NIGHTLY = "nightly"
    private const val PRODUCT_FLAVOR_KLAR = "klar"

    val isKlarBuild: Boolean
        get() = PRODUCT_FLAVOR_KLAR == BuildConfig.FLAVOR

    val isReleaseBuild: Boolean
        get() = BUILD_TYPE_RELEASE == BuildConfig.BUILD_TYPE

    val isBetaBuild: Boolean
        get() = BUILD_TYPE_BETA == BuildConfig.BUILD_TYPE

    val isNightlyBuild: Boolean
        get() = BUILD_TYPE_NIGHTLY == BuildConfig.BUILD_TYPE

    val isDevBuild: Boolean
        get() = BUILD_TYPE_DEBUG == BuildConfig.BUILD_TYPE

    val isDevOrNightlyBuild: Boolean
        get() = isDevBuild || isNightlyBuild
}
