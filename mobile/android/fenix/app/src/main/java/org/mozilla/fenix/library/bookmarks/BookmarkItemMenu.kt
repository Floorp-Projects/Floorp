/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import android.content.Context
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import org.mozilla.fenix.DefaultThemeManager
import org.mozilla.fenix.R

class BookmarkItemMenu(
    private val context: Context,
    private val onItemTapped: (BookmarkItemMenu.Item) -> Unit = {}
) {

    sealed class Item {
        object Edit : Item()
        object Select : Item()
        object Copy : Item()
        object Share : Item()
        object OpenInNewTab : Item()
        object OpenInPrivateTab : Item()
        object Delete : Item()
    }

    val menuBuilder by lazy { BrowserMenuBuilder(menuItems) }

    private val menuItems by lazy {
        listOf(
            SimpleBrowserMenuItem(context.getString(R.string.bookmark_menu_edit_button)) {
                onItemTapped.invoke(BookmarkItemMenu.Item.Edit)
            },
            SimpleBrowserMenuItem(context.getString(R.string.bookmark_menu_select_button)) {
                onItemTapped.invoke(BookmarkItemMenu.Item.Select)
            },
            SimpleBrowserMenuItem(context.getString(R.string.bookmark_menu_copy_button)) {
                onItemTapped.invoke(BookmarkItemMenu.Item.Copy)
            },
            SimpleBrowserMenuItem(context.getString(R.string.bookmark_menu_share_button)) {
                onItemTapped.invoke(BookmarkItemMenu.Item.Share)
            },
            SimpleBrowserMenuItem(context.getString(R.string.bookmark_menu_open_in_new_tab_button)) {
                onItemTapped.invoke(BookmarkItemMenu.Item.OpenInNewTab)
            },
            SimpleBrowserMenuItem(context.getString(R.string.bookmark_menu_open_in_private_tab_button)) {
                onItemTapped.invoke(BookmarkItemMenu.Item.OpenInPrivateTab)
            },
            SimpleBrowserMenuItem(
                context.getString(R.string.bookmark_menu_delete_button),
                textColorResource = DefaultThemeManager.resolveAttribute(
                    R.attr.deleteColor,
                    context
                )
            ) {
                onItemTapped.invoke(BookmarkItemMenu.Item.Delete)
            }
        )
    }
}
