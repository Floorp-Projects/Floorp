/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import android.content.Context
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import org.mozilla.fenix.R

class SelectionMenu(
    private val context: Context,
    private val onItemTapped: (Item) -> Unit = {}
) {
    sealed class Item {
        object BookmarkTabs : Item()
        object DeleteTabs : Item()
    }

    val menuBuilder by lazy { BrowserMenuBuilder(menuItems) }

    private val menuItems by lazy {
        listOf(
            SimpleBrowserMenuItem(
                context.getString(R.string.tab_tray_multiselect_menu_item_bookmark),
                textColorResource = R.color.primary_text_normal_theme
            ) {
                onItemTapped.invoke(Item.BookmarkTabs)
            },

            SimpleBrowserMenuItem(
                context.getString(R.string.tab_tray_multiselect_menu_item_close),
                textColorResource = R.color.primary_text_normal_theme
            ) {
                onItemTapped.invoke(Item.DeleteTabs)
            }
        )
    }
}
