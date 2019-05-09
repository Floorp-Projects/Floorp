/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

enum class ReleaseChannel {
    Debug, Nightly, Beta, Release;

    val isReleased: Boolean
        get() = when (this) {
            Release, Beta, Nightly -> true
            else -> false
        }
}

object Config {
    val channel = when (BuildConfig.BUILD_TYPE) {
        "release" -> ReleaseChannel.Release
        "beta" -> ReleaseChannel.Beta
        "nightly" -> ReleaseChannel.Nightly
        else -> ReleaseChannel.Debug
    }
}
