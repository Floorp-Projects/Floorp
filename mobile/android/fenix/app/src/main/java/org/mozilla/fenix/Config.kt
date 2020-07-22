/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

enum class ReleaseChannel {
    Debug,
    Nightly,
    Beta,
    Release;

    val isReleased: Boolean
        get() = when (this) {
            Debug -> false
            else -> true
        }

    /**
     * True if this is a debug release channel, false otherwise.
     *
     * This constant should often be used instead of [BuildConfig.DEBUG], which indicates
     * if the `debuggable` flag is set which can be true even on released channel builds
     * (e.g. performance).
     */
    val isDebug: Boolean
        get() = !this.isReleased

    val isReleaseOrBeta: Boolean
        get() = this == Release || this == Beta

    val isRelease: Boolean
        get() = when (this) {
            Release -> true
            else -> false
        }

    val isBeta: Boolean
        get() = this == Beta

    val isNightlyOrDebug: Boolean
        get() = this == Debug || this == Nightly

    /**
     * Is this a build for a release channel that we used to ship Fennec on?
     */
    val isFennec: Boolean
        get() = this in fennecChannels

    /**
     * Is this build for a "pure" Fenix channel that we never shipped Fennec on?
     */
    val isFenix: Boolean
        get() = !isFennec
}

object Config {
    val channel = when (BuildConfig.BUILD_TYPE) {
        "debug" -> ReleaseChannel.Debug
        "nightly" -> ReleaseChannel.Nightly
        "beta" -> ReleaseChannel.Beta
        "release" -> ReleaseChannel.Release
        else -> {
            throw IllegalStateException("Unknown build type: ${BuildConfig.BUILD_TYPE}")
        }
    }
}

private val fennecChannels: List<ReleaseChannel> = listOf(
    ReleaseChannel.Beta,
    ReleaseChannel.Release
)
