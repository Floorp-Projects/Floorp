/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.menu

import androidx.annotation.ColorInt
import mozilla.components.concept.menu.candidate.MenuEffect
import mozilla.components.support.base.observer.Observable

/**
 * A `three-dot` button used for expanding menus.
 *
 * If you are using a browser toolbar, do not use this class directly.
 */
interface MenuButton : Observable<MenuButton.Observer> {

    /**
     * Sets a [MenuController] that will be used to create a menu when this button is clicked.
     */
    var menuController: MenuController?

    /**
     * Show the indicator for a browser menu effect.
     */
    fun setEffect(effect: MenuEffect?)

    /**
     * Sets the tint of the 3-dot menu icon.
     */
    fun setColorFilter(@ColorInt color: Int)

    /**
     * Observer for the menu button.
     */
    interface Observer {
        /**
         * Listener called when the menu is shown.
         */
        fun onShow() = Unit

        /**
         * Listener called when the menu is dismissed.
         */
        fun onDismiss() = Unit
    }
}
