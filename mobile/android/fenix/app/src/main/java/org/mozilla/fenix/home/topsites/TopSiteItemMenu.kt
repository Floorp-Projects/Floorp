/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.topsites

import android.content.Context
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import org.mozilla.fenix.R

class TopSiteItemMenu(
    private val context: Context,
    private val isPinnedSite: Boolean,
    private val onItemTapped: (Item) -> Unit = {}
) {
    sealed class Item {
        object OpenInPrivateTab : Item()
        object RenameTopSite : Item()
        object RemoveTopSite : Item()
    }

    val menuBuilder by lazy { BrowserMenuBuilder(menuItems) }

    private val menuItems by lazy {
        listOfNotNull(
            SimpleBrowserMenuItem(
                context.getString(R.string.bookmark_menu_open_in_private_tab_button)
            ) {
                onItemTapped.invoke(Item.OpenInPrivateTab)
            },
            if (isPinnedSite) SimpleBrowserMenuItem(
                context.getString(R.string.rename_top_site)
            ) {
                onItemTapped.invoke(Item.RenameTopSite)
            } else null,
            SimpleBrowserMenuItem(
                if (isPinnedSite) {
                    context.getString(R.string.remove_top_site)
                } else {
                    context.getString(R.string.delete_from_history)
                }
            ) {
                onItemTapped.invoke(Item.RemoveTopSite)
            }
        )
    }
}
