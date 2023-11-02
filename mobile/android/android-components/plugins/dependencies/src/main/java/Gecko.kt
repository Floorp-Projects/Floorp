/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Gecko version and release channel constants used by this version of Android Components.
 */
object Gecko {
    /**
     * GeckoView Version.
     * This field is changed from 'const val' to a function in order to fix the bug sometimes happening
     * in the gradle sync process probably due to Gradle's incremental compilation feature. This change
     * ensures re-evaluation of this file when Gradle is re-parsing gradle files while syncing.
     */
    fun version() = "121.0.20231116093942"

    /**
     * GeckoView channel
     * This field is changed from 'const val' to a function in order to fix the bug sometimes happening
     * in the gradle sync process probably due to Gradle's incremental compilation feature. This change
     * ensures re-evaluation of this file when Gradle is re-parsing gradle files while syncing.
     */
    fun channel() = GeckoChannel.NIGHTLY
}

/**
 * Enum for GeckoView release channels.
 */
enum class GeckoChannel(
    val artifactName: String,
) {
    NIGHTLY("geckoview-nightly-omni"),
    BETA("geckoview-beta-omni"),
    RELEASE("geckoview-omni"),
}
