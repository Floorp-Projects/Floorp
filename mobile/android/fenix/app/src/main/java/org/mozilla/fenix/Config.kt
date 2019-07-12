/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

enum class ReleaseChannel {
    Debug, Nightly, Beta, Production;

    val isReleased: Boolean
        get() = when (this) {
            Debug -> false
            else -> true
        }
}

object Config {
    val channel = when (BuildConfig.BUILD_TYPE) {
        "production" -> ReleaseChannel.Production
        "beta" -> ReleaseChannel.Beta
        "nightly" -> ReleaseChannel.Nightly
        "nightlyLegacy" -> ReleaseChannel.Nightly
        "debug" -> ReleaseChannel.Debug
        else -> ReleaseChannel.Production // Performance-test builds should test production behaviour
    }
}
