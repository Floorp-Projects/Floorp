/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabtray

import android.content.Context
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components

class MultiselectSelectionMenu(
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
                context.components.analytics.metrics.track(Event.TabsTraySaveToCollectionPressed)
                onItemTapped.invoke(Item.BookmarkTabs)
            },

            SimpleBrowserMenuItem(
                context.getString(R.string.tab_tray_multiselect_menu_item_close),
                textColorResource = R.color.primary_text_normal_theme
            ) {
                context.components.analytics.metrics.track(Event.TabsTrayShareAllTabsPressed)
                onItemTapped.invoke(Item.DeleteTabs)
            }
        )
    }
}
