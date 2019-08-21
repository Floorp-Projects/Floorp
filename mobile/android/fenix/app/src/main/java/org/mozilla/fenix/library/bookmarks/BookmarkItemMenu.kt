/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import android.content.Context
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import org.mozilla.fenix.R
import org.mozilla.fenix.theme.ThemeManager
import org.mozilla.fenix.library.LibraryItemMenu

class BookmarkItemMenu(
    private val context: Context,
    private val item: BookmarkNode,
    private val onItemTapped: (BookmarkItemMenu.Item) -> Unit = {}
) : LibraryItemMenu {

    sealed class Item {
        object Edit : Item()
        object Select : Item()
        object Copy : Item()
        object Share : Item()
        object OpenInNewTab : Item()
        object OpenInPrivateTab : Item()
        object Delete : Item()
    }

    override val menuBuilder by lazy { BrowserMenuBuilder(menuItems) }

    private val menuItems by lazy {
        listOfNotNull(
            if (item.type in listOf(BookmarkNodeType.ITEM, BookmarkNodeType.FOLDER)) {
                SimpleBrowserMenuItem(context.getString(R.string.bookmark_menu_edit_button)) {
                    onItemTapped.invoke(BookmarkItemMenu.Item.Edit)
                }
            } else null,
            if (item.type == BookmarkNodeType.ITEM) {
                SimpleBrowserMenuItem(context.getString(R.string.bookmark_menu_copy_button)) {
                    onItemTapped.invoke(BookmarkItemMenu.Item.Copy)
                }
            } else null,
            if (item.type == BookmarkNodeType.ITEM) {
                SimpleBrowserMenuItem(context.getString(R.string.bookmark_menu_share_button)) {
                    onItemTapped.invoke(BookmarkItemMenu.Item.Share)
                }
            } else null,
            if (item.type == BookmarkNodeType.ITEM) {
                SimpleBrowserMenuItem(context.getString(R.string.bookmark_menu_open_in_new_tab_button)) {
                    onItemTapped.invoke(BookmarkItemMenu.Item.OpenInNewTab)
                }
            } else null,
            if (item.type == BookmarkNodeType.ITEM) {
                SimpleBrowserMenuItem(context.getString(R.string.bookmark_menu_open_in_private_tab_button)) {
                    onItemTapped.invoke(BookmarkItemMenu.Item.OpenInPrivateTab)
                }
            } else null,
            SimpleBrowserMenuItem(
                context.getString(R.string.bookmark_menu_delete_button),
                textColorResource = ThemeManager.resolveAttribute(R.attr.destructive, context)
            ) {
                onItemTapped.invoke(BookmarkItemMenu.Item.Delete)
            }
        )
    }
}
