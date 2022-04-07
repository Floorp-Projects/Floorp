/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import android.content.Context
import androidx.annotation.VisibleForTesting
import mozilla.components.browser.menu2.BrowserMenuController
import mozilla.components.concept.menu.MenuController
import mozilla.components.concept.menu.candidate.TextMenuCandidate
import mozilla.components.concept.menu.candidate.TextStyle
import mozilla.components.concept.storage.BookmarkNodeType
import mozilla.components.support.ktx.android.content.getColorFromAttr
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.bookmarkStorage

class BookmarkItemMenu(
    private val context: Context,
    private val onItemTapped: (Item) -> Unit,
) {

    enum class Item {
        Edit,
        Copy,
        Share,
        OpenInNewTab,
        OpenInPrivateTab,
        OpenAllInTabs,
        OpenAllInPrivateTabs,
        Delete,
        ;
    }

    val menuController: MenuController by lazy { BrowserMenuController() }

    /**
     * Check if the menu item has to be displayed or not for the type of bookmark.
     * If wanted, return the item.
     * Else, return null.
     */
    private fun maybeCreateMenuItem(
        itemType: BookmarkNodeType,
        wantedType: BookmarkNodeType,
        text: String,
        action: Item,
    ): TextMenuCandidate? {
        return maybeCreateMenuItem(itemType, listOf(wantedType), text, action)
    }

    private fun maybeCreateMenuItem(
        itemType: BookmarkNodeType,
        wantedTypes: List<BookmarkNodeType>,
        text: String,
        action: Item,
    ): TextMenuCandidate? {
        return if (itemType in wantedTypes) {
            TextMenuCandidate(
                text = text,
            ) {
                onItemTapped.invoke(action)
            }
        } else {
            null
        }
    }

    @VisibleForTesting
    internal suspend fun menuItems(itemType: BookmarkNodeType, itemId: String): List<TextMenuCandidate> {
        // if have at least one child
        val hasAtLeastOneChild = !context.bookmarkStorage.getTree(itemId)?.children.isNullOrEmpty()

        return listOfNotNull(
            maybeCreateMenuItem(
                itemType,
                listOf(BookmarkNodeType.ITEM, BookmarkNodeType.FOLDER),
                context.getString(R.string.bookmark_menu_edit_button),
                Item.Edit,
            ),
            maybeCreateMenuItem(
                itemType,
                BookmarkNodeType.ITEM,
                context.getString(R.string.bookmark_menu_copy_button),
                Item.Copy,
            ),
            maybeCreateMenuItem(
                itemType,
                BookmarkNodeType.ITEM,
                context.getString(R.string.bookmark_menu_share_button),
                Item.Share,
            ),
            maybeCreateMenuItem(
                itemType,
                BookmarkNodeType.ITEM,
                context.getString(R.string.bookmark_menu_open_in_new_tab_button),
                Item.OpenInNewTab,
            ),
            maybeCreateMenuItem(
                itemType,
                BookmarkNodeType.ITEM,
                context.getString(R.string.bookmark_menu_open_in_private_tab_button),
                Item.OpenInPrivateTab,
            ),
            if (hasAtLeastOneChild) {
                maybeCreateMenuItem(
                    itemType,
                    BookmarkNodeType.FOLDER,
                    context.getString(R.string.bookmark_menu_open_all_in_tabs_button),
                    Item.OpenAllInTabs,
                )
            } else {
                null
            },
            if (hasAtLeastOneChild) {
                maybeCreateMenuItem(
                    itemType,
                    BookmarkNodeType.FOLDER,
                    context.getString(R.string.bookmark_menu_open_all_in_privates_button),
                    Item.OpenAllInPrivateTabs,
                )
            } else {
                null
            },
            TextMenuCandidate(
                text = context.getString(R.string.bookmark_menu_delete_button),
                textStyle = TextStyle(color = context.getColorFromAttr(R.attr.textWarning)),
            ) {
                onItemTapped.invoke(Item.Delete)
            },
        )
    }

    suspend fun updateMenu(itemType: BookmarkNodeType, itemId: String) {
        menuController.submitList(menuItems(itemType, itemId))
    }
}
