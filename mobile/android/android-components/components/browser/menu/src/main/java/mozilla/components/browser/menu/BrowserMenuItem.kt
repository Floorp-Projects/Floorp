/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu

import android.view.View

/**
 * Interface to be implemented by menu items to be shown in the browser menu.
 */
interface BrowserMenuItem {
    /**
     * Lambda expression that returns true if this item should be shown in the menu. Returns false
     * if this item should be hidden.
     */
    val visible: () -> Boolean

    /**
     * Returns the layout resource ID of the layout to be inflated for showing a menu item of this
     * type.
     */
    fun getLayoutResource(): Int

    /**
     * Called by the browser menu to display the data of this item using the passed view.
     */
    fun bind(menu: BrowserMenu, view: View)

    /**
     * Called by the browser menu to update the displayed data of this item using the passed view.
     */
    fun invalidate(view: View) = Unit
}
