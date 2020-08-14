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

class BookmarkItemMenu(
    private val context: Context,
    private val onItemTapped: (Item) -> Unit
) {

    enum class Item {
        Edit,
        Copy,
        Share,
        OpenInNewTab,
        OpenInPrivateTab,
        Delete;
    }

    val menuController: MenuController by lazy { BrowserMenuController() }

    @VisibleForTesting
    internal fun menuItems(itemType: BookmarkNodeType): List<TextMenuCandidate> {
        return listOfNotNull(
            if (itemType != BookmarkNodeType.SEPARATOR) {
                TextMenuCandidate(
                    text = context.getString(R.string.bookmark_menu_edit_button)
                ) {
                    onItemTapped.invoke(Item.Edit)
                }
            } else {
                null
            },
            if (itemType == BookmarkNodeType.ITEM) {
                TextMenuCandidate(
                    text = context.getString(R.string.bookmark_menu_copy_button)
                ) {
                    onItemTapped.invoke(Item.Copy)
                }
            } else {
                null
            },
            if (itemType == BookmarkNodeType.ITEM) {
                TextMenuCandidate(
                    text = context.getString(R.string.bookmark_menu_share_button)
                ) {
                    onItemTapped.invoke(Item.Share)
                }
            } else {
                null
            },
            if (itemType == BookmarkNodeType.ITEM) {
                TextMenuCandidate(
                    text = context.getString(R.string.bookmark_menu_open_in_new_tab_button)
                ) {
                    onItemTapped.invoke(Item.OpenInNewTab)
                }
            } else {
                null
            },
            if (itemType == BookmarkNodeType.ITEM) {
                TextMenuCandidate(
                    text = context.getString(R.string.bookmark_menu_open_in_private_tab_button)
                ) {
                    onItemTapped.invoke(Item.OpenInPrivateTab)
                }
            } else {
                null
            },
            TextMenuCandidate(
                text = context.getString(R.string.bookmark_menu_delete_button),
                textStyle = TextStyle(color = context.getColorFromAttr(R.attr.destructive))
            ) {
                onItemTapped.invoke(Item.Delete)
            }
        )
    }

    fun updateMenu(itemType: BookmarkNodeType) {
        menuController.submitList(menuItems(itemType))
    }
}
