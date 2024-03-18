/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components

import mozilla.components.support.base.BuildConfig

/**
 * Information about the current Android Components build.
 */
object Build {
    /**
     * The version name of this Android Components release (e.g. 0.54.0 or 0.55.0-SNAPSHOT).
     */
    const val version: String = BuildConfig.LIBRARY_VERSION

    /**
     * The version of "Application Services" libraries this version was *build* against.
     *
     * Note that a consuming app may be able to override the actual version that is used at runtime.
     */
    const val applicationServicesVersion: String = BuildConfig.APPLICATION_SERVICES_VERSION

    /**
     * The version of the "Glean SDK" library this version was *build* against.
     */
    const val gleanSdkVersion: String = BuildConfig.GLEAN_SDK_VERSION

    /**
     * Git hash of the latest commit in the Android Components repository checkout this version was built from.
     */
    const val gitHash: String = BuildConfig.GIT_HASH
}
