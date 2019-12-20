/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import io.sentry.Sentry

enum class ReleaseChannel {
    FenixDebug, FenixNightly, FenixBeta, FenixProduction, FennecProduction;

    val isReleased: Boolean
        get() = when (this) {
            FenixDebug -> false
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
        get() = when (this) {
            FenixProduction -> true
            FenixBeta -> true
            else -> false
        }

    val isNightlyOrDebug: Boolean
        get() = when (this) {
            FenixNightly -> true
            FenixDebug -> true
            else -> false
        }
}

object Config {
    val channel = when (BuildConfig.BUILD_TYPE) {
        "fenixProduction" -> ReleaseChannel.FenixProduction
        "fenixBeta" -> ReleaseChannel.FenixBeta
        "fenixNightly" -> ReleaseChannel.FenixNightly
        "debug" -> ReleaseChannel.FenixDebug
        "fennecProduction" -> ReleaseChannel.FennecProduction
        else -> {
            Sentry.capture("BuildConfig.BUILD_TYPE ${BuildConfig.BUILD_TYPE} did not match expected channels")
            // Performance-test builds should test production behaviour
            ReleaseChannel.FenixProduction
        }
    }
}
