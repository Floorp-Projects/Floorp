/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import android.view.View
import androidx.annotation.VisibleForTesting
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.BrowserMenuItem

/**
 * An abstract menu item for handling nested sub menu items on view click.
 *
 * @param subMenu Target sub menu to be shown when this menu item is clicked.
 * @param endOfMenuAlwaysVisible when is set to true makes sure the bottom of the menu is always visible
 * otherwise, the top of the menu is always visible.
 * @param isCollapsingMenuLimit Whether this menu item can serve as the limit of a collapsing menu.
 * @param isSticky whether this item menu should not be scrolled offscreen (downwards or upwards
 * depending on the menu position).
 */
abstract class AbstractParentBrowserMenuItem(
    private val subMenu: BrowserMenu,
    private val endOfMenuAlwaysVisible: Boolean,
    override val isCollapsingMenuLimit: Boolean = false,
    override val isSticky: Boolean = false,
) : BrowserMenuItem {
    /**
     * Listener called when the sub menu is shown.
     */
    var onSubMenuShow: () -> Unit = {}

    /**
     * Listener called when the sub menu is dismissed.
     */
    var onSubMenuDismiss: () -> Unit = {}
    abstract override var visible: () -> Boolean
    abstract override fun getLayoutResource(): Int

    override fun bind(menu: BrowserMenu, view: View) {
        view.setOnClickListener {
            menu.dismiss()
            subMenu.show(
                anchor = menu.currAnchor ?: view,
                orientation = BrowserMenu.determineMenuOrientation(view.parent as? View?),
                endOfMenuAlwaysVisible = endOfMenuAlwaysVisible,
            ) {
                onSubMenuDismiss()
            }
            onSubMenuShow()
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    internal fun onBackPressed(menu: BrowserMenu, view: View) {
        if (subMenu.isShown) {
            subMenu.dismiss()
            onSubMenuDismiss()
            menu.show(
                anchor = menu.currAnchor ?: view,
                orientation = BrowserMenu.determineMenuOrientation(view.parent as? View?),
                endOfMenuAlwaysVisible = endOfMenuAlwaysVisible,
            )
        }
    }
}
