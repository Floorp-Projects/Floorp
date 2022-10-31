/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.feature

/**
 * Generic interface for fragments, features and other components that want to handle 'back' button presses.
 */
@Deprecated("Use `UserInteractionHandler` instead.")
interface BackHandler {
    /**
     * Called when this [UserInteractionHandler] gets the option to handle the user pressing the back key.
     *
     * Returns true if this [UserInteractionHandler] consumed the event and no other components need to be notified.
     */
    @Deprecated(
        "Use `UserInteractionHandler` instead.",
        ReplaceWith(
            "onBackPressed()",
            "mozilla.components.support.base.feature.UserInteractionHandler",
        ),
    )
    fun onBackPressed(): Boolean
}
