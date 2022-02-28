/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine.activity

import android.content.pm.ActivityInfo

/**
 * Notifies applications or other components of engine orientation lock events.
 */
interface OrientationDelegate {
    /**
     * Request to force a certain screen orientation on the current activity.
     *
     * @param requestedOrientation The screen orientation which should be set.
     * Values can be any of screen orientation values defined in [ActivityInfo].
     *
     * @return Whether the request to set a screen orientation is promised to be fulfilled or denied.
     */
    fun onOrientationLock(requestedOrientation: Int): Boolean = true

    /**
     * Request to restore the natural device orientation, what it was before [onOrientationLock].
     * Implementers should usually set [ActivityInfo.SCREEN_ORIENTATION_UNSPECIFIED].
     */
    fun onOrientationUnlock() = Unit
}
