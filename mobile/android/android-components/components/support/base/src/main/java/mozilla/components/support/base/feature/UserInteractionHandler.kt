/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.feature

import android.app.Activity

/**
 * Generic interface for fragments, features and other components that want to handle user
 * interactions such as 'back' or 'home' button presses.
 */
interface UserInteractionHandler {
    /**
     * Called when this [UserInteractionHandler] gets the option to handle the user pressing the back key.
     *
     * Returns true if this [UserInteractionHandler] consumed the event and no other components need to be notified.
     */
    fun onBackPressed(): Boolean

    /**
     * In most cases, when the home button is pressed, we invoke this callback to inform the app that the user
     * is going to leave the app.
     *
     * See also [Activity.onUserLeaveHint] for more details.
     */
    fun onHomePressed(): Boolean = false
}
