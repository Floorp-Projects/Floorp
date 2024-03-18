/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import android.content.Context
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import org.mozilla.fenix.Config
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components

class SelectionMenu(
    private val context: Context,
    private val onItemTapped: (Item) -> Unit = {},
) {
    sealed class Item {
        object BookmarkTabs : Item()
        object DeleteTabs : Item()
        object MakeInactive : Item()
    }

    val menuBuilder by lazy { BrowserMenuBuilder(menuItems) }

    private val menuItems by lazy {
        listOf(
            SimpleBrowserMenuItem(
                context.getString(R.string.tab_tray_multiselect_menu_item_bookmark),
                textColorResource = R.color.fx_mobile_text_color_primary,
            ) {
                onItemTapped.invoke(Item.BookmarkTabs)
            },

            SimpleBrowserMenuItem(
                context.getString(R.string.tab_tray_multiselect_menu_item_close),
                textColorResource = R.color.fx_mobile_text_color_primary,
            ) {
                onItemTapped.invoke(Item.DeleteTabs)
            },
            // This item is only visible for debugging.
            SimpleBrowserMenuItem(
                context.getString(R.string.inactive_tabs_menu_item),
                textColorResource = R.color.fx_mobile_text_color_primary,
            ) {
                onItemTapped.invoke(Item.MakeInactive)
            }.apply {
                // We only want this menu option visible when in debug mode for testing.
                visible = { Config.channel.isDebug || context.components.settings.showSecretDebugMenuThisSession }
            },
        )
    }
}
