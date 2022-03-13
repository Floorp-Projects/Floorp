/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.downloads

import android.content.Context
import androidx.annotation.VisibleForTesting
import mozilla.components.browser.menu2.BrowserMenuController
import mozilla.components.concept.menu.MenuController
import mozilla.components.concept.menu.candidate.TextMenuCandidate
import mozilla.components.concept.menu.candidate.TextStyle
import mozilla.components.support.ktx.android.content.getColorFromAttr
import org.mozilla.fenix.R

class DownloadItemMenu(
    private val context: Context,
    private val onItemTapped: (Item) -> Unit
) {

    enum class Item {
        Delete
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
                text = context.getString(R.string.history_delete_item),
                textStyle = TextStyle(
                    color = context.getColorFromAttr(R.attr.textWarning)
                )
            ) {
                onItemTapped.invoke(Item.Delete)
            }
        )
    }
}
