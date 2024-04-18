/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.android

import android.os.Build

/**
 * This class provides information about the build version without exposing the android framework
 * APIs directly, making it easier to test the code that depends on it.
 */
interface BuildVersionProvider {

    /**
     * Returns the SDK_INT of the current build version.
     */
    fun sdkInt(): Int

    companion object {
        const val FOREGROUND_SERVICE_RESTRICTIONS_STARTING_VERSION = Build.VERSION_CODES.S
    }
}

/**
 * @see BuildVersionProvider
 */
class DefaultBuildVersionProvider : BuildVersionProvider {

    override fun sdkInt(): Int = Build.VERSION.SDK_INT
}
