/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.menu.item

import android.view.View
import androidx.annotation.ColorRes
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.BrowserMenuItem
import mozilla.components.browser.menu.R

/**
 * A browser menu item that is to be used only as a placeholder for inserting web extensions in main menu.
 * The id of the web extension to be inserted has to correspond to the id of the browser menu item.
 *
 * @param id The id of this menu item.
 * @param iconTintColorResource Optional ID of color resource to tint the icon.
 */
class WebExtensionPlaceholderMenuItem(
    val id: String,
    @ColorRes
    val iconTintColorResource: Int = NO_ID,
) : BrowserMenuItem {
    override var visible: () -> Boolean = { false }

    override fun getLayoutResource() = R.layout.mozac_browser_menu_item_simple

    override fun bind(menu: BrowserMenu, view: View) {
        // no binding, item not visible.
    }

    companion object {
        const val MAIN_EXTENSIONS_MENU_ID = "mainExtensionsMenu"
    }
}
