/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import android.content.Context
import androidx.annotation.VisibleForTesting
import mozilla.components.browser.menu2.BrowserMenuController
import mozilla.components.concept.menu.MenuController
import mozilla.components.concept.menu.candidate.TextMenuCandidate
import mozilla.components.concept.menu.candidate.TextStyle
import mozilla.components.support.ktx.android.content.getColorFromAttr
import org.mozilla.fenix.R

class HistoryItemMenu(
    private val context: Context,
    private val onItemTapped: (Item) -> Unit
) {

    enum class Item {
        Copy,
        Share,
        OpenInNewTab,
        OpenInPrivateTab,
        Delete;
    }

    val menuController: MenuController by lazy {
        BrowserMenuController().apply {
            submitList(menuItems())
        }
    }

    @VisibleForTesting
    internal fun menuItems(): List<TextMenuCandidate> {
        return listOf(
            TextMenuCandidate(
                text = context.getString(R.string.history_menu_copy_button)
            ) {
                onItemTapped.invoke(Item.Copy)
            },
            TextMenuCandidate(
                text = context.getString(R.string.history_menu_share_button)
            ) {
                onItemTapped.invoke(Item.Share)
            },
            TextMenuCandidate(
                text = context.getString(R.string.history_menu_open_in_new_tab_button)
            ) {
                onItemTapped.invoke(Item.OpenInNewTab)
            },
            TextMenuCandidate(
                text = context.getString(R.string.history_menu_open_in_private_tab_button)
            ) {
                onItemTapped.invoke(Item.OpenInPrivateTab)
            },
            TextMenuCandidate(
                text = context.getString(R.string.history_delete_item),
                textStyle = TextStyle(
                    color = context.getColorFromAttr(R.attr.destructive)
                )
            ) {
                onItemTapped.invoke(Item.Delete)
            }
        )
    }
}
