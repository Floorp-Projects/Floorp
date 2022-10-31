/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu

import android.content.Context
import android.view.View
import mozilla.components.browser.menu.view.ExpandableLayout
import mozilla.components.browser.menu.view.StickyItemsLinearLayoutManager
import mozilla.components.concept.menu.candidate.MenuCandidate

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
     * Lambda expression that returns the number of interactive elements in this menu item.
     * For example, a simple item will have 1, divider will have 0, and a composite
     * item, like a tool bar, will have several.
     */
    val interactiveCount: () -> Int get() = { 1 }

    /**
     * Whether this menu item can serve as the limit of a collapsing menu.
     *
     * @see [ExpandableLayout]
     */
    val isCollapsingMenuLimit: Boolean get() = false

    /**
     * Whether this menu item should not be scrollable off-screen.
     *
     * @see [StickyItemsLinearLayoutManager]
     */
    val isSticky: Boolean get() = false

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

    /**
     * Converts the menu item into a menu candidate.
     */
    fun asCandidate(context: Context): MenuCandidate? = null
}
