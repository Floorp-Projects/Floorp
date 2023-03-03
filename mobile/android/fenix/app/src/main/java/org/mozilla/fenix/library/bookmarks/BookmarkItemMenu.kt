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
        OpenAllInNewTabs,
        OpenAllInPrivateTabs,
        Delete,
    }

    val menuController: MenuController by lazy { BrowserMenuController() }

    @VisibleForTesting
    @SuppressWarnings("LongMethod")
    internal suspend fun menuItems(itemType: BookmarkNodeType, itemId: String): List<TextMenuCandidate> {
        val hasAtLeastOneChild = !context.bookmarkStorage.getTree(itemId)?.children.isNullOrEmpty()

        return listOfNotNull(
            if (itemType != BookmarkNodeType.SEPARATOR) {
                TextMenuCandidate(
                    text = context.getString(R.string.bookmark_menu_edit_button),
                ) {
                    onItemTapped.invoke(Item.Edit)
                }
            } else {
                null
            },
            if (itemType == BookmarkNodeType.ITEM) {
                TextMenuCandidate(
                    text = context.getString(R.string.bookmark_menu_copy_button),
                ) {
                    onItemTapped.invoke(Item.Copy)
                }
            } else {
                null
            },
            if (itemType == BookmarkNodeType.ITEM) {
                TextMenuCandidate(
                    text = context.getString(R.string.bookmark_menu_share_button),
                ) {
                    onItemTapped.invoke(Item.Share)
                }
            } else {
                null
            },
            if (itemType == BookmarkNodeType.ITEM) {
                TextMenuCandidate(
                    text = context.getString(R.string.bookmark_menu_open_in_new_tab_button),
                ) {
                    onItemTapped.invoke(Item.OpenInNewTab)
                }
            } else {
                null
            },
            if (itemType == BookmarkNodeType.ITEM) {
                TextMenuCandidate(
                    text = context.getString(R.string.bookmark_menu_open_in_private_tab_button),
                ) {
                    onItemTapped.invoke(Item.OpenInPrivateTab)
                }
            } else {
                null
            },
            if (hasAtLeastOneChild && itemType == BookmarkNodeType.FOLDER) {
                TextMenuCandidate(
                    text = context.getString(R.string.bookmark_menu_open_all_in_tabs_button),
                ) {
                    onItemTapped.invoke(Item.OpenAllInNewTabs)
                }
            } else {
                null
            },
            if (hasAtLeastOneChild && itemType == BookmarkNodeType.FOLDER) {
                TextMenuCandidate(
                    text = context.getString(R.string.bookmark_menu_open_all_in_private_tabs_button),
                ) {
                    onItemTapped.invoke(Item.OpenAllInPrivateTabs)
                }
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

    /**
     * Update the menu items for the type of bookmark.
     */
    suspend fun updateMenu(itemType: BookmarkNodeType, itemId: String) {
        menuController.submitList(menuItems(itemType, itemId))
    }
}
