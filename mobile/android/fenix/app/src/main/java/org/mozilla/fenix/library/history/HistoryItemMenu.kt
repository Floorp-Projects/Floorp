/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import android.content.Context
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import org.mozilla.fenix.R
import org.mozilla.fenix.library.LibraryItemMenu
import org.mozilla.fenix.theme.ThemeManager

class HistoryItemMenu(
    private val context: Context,
    private val onItemTapped: (Item) -> Unit = {}
) : LibraryItemMenu {
    sealed class Item {
        object Copy : Item()
        object Share : Item()
        object OpenInNewTab : Item()
        object OpenInPrivateTab : Item()
        object Delete : Item()
    }

    override val menuBuilder by lazy { BrowserMenuBuilder(menuItems) }

    private val menuItems by lazy {
        listOfNotNull(
            SimpleBrowserMenuItem(context.getString(R.string.history_menu_copy_button)) {
                onItemTapped.invoke(Item.Copy)
            },
            SimpleBrowserMenuItem(context.getString(R.string.history_menu_share_button)) {
                onItemTapped.invoke(Item.Share)
            },
            SimpleBrowserMenuItem(context.getString(R.string.history_menu_open_in_new_tab_button)) {
                onItemTapped.invoke(Item.OpenInNewTab)
            },
            SimpleBrowserMenuItem(context.getString(R.string.history_menu_open_in_private_tab_button)) {
                onItemTapped.invoke(Item.OpenInPrivateTab)
            },
            SimpleBrowserMenuItem(
                context.getString(R.string.history_delete_item),
                textColorResource = ThemeManager.resolveAttribute(R.attr.destructive, context)
            ) {
                onItemTapped.invoke(Item.Delete)
            }
        )
    }
}
