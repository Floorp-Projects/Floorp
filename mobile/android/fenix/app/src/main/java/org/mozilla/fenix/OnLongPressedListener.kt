/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

/**
 * Interface for features and fragments that want to handle long presses of the system back/forward button
 */
interface OnLongPressedListener {

    /**
     * Called when the system back button is long pressed.
     *
     * Note: This cannot be called when gesture navigation is enabled on Android 10+ due to system
     * limitations.
     *
     * @return true if the event was handled
     */
    fun onBackLongPressed(): Boolean

    /**
     * Called when the system forward button is long pressed.
     *
     * @return true if the event was handled
     */
    fun onForwardLongPressed(): Boolean
}
