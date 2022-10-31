/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.menu

import android.view.View
import android.widget.PopupWindow
import mozilla.components.concept.menu.candidate.MenuCandidate
import mozilla.components.support.base.observer.Observable

/**
 * Controls a popup menu composed of MenuCandidate objects.
 */
interface MenuController : Observable<MenuController.Observer> {

    /**
     * @param anchor The view on which to pin the popup window.
     * @param orientation The preferred orientation to show the popup window.
     * @param forceOrientation When set to true, the orientation will be respected even when the
     * menu doesn't fully fit.
     */
    fun show(anchor: View, orientation: Orientation? = null, forceOrientation: Boolean = false): PopupWindow

    /**
     * Dismiss the menu popup if the menu is visible.
     */
    fun dismiss()

    /**
     * Changes the contents of the menu.
     */
    fun submitList(list: List<MenuCandidate>)

    /**
     * Observer for the menu controller.
     */
    interface Observer {
        /**
         * Called when the menu contents have changed.
         */
        fun onMenuListSubmit(list: List<MenuCandidate>) = Unit

        /**
         * Called when the menu has been dismissed.
         */
        fun onDismiss() = Unit
    }
}
