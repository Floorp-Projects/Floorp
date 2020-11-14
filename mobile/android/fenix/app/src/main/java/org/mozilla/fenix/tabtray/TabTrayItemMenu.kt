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

class TabTrayItemMenu(
    private val context: Context,
    private val shouldShowShareAllTabs: () -> Boolean,
    private val shouldShowSelectTabs: () -> Boolean,
    private val hasOpenTabs: () -> Boolean,
    private val onItemTapped: (Item) -> Unit = {}
) {

    sealed class Item {
        object ShareAllTabs : Item()
        object OpenTabSettings : Item()
        object SelectTabs : Item()
        object CloseAllTabs : Item()
        object OpenRecentlyClosed : Item()
    }

    val menuBuilder by lazy { BrowserMenuBuilder(menuItems) }

    private val menuItems by lazy {
        listOf(
            SimpleBrowserMenuItem(
                context.getString(R.string.tabs_tray_select_tabs),
                textColorResource = R.color.primary_text_normal_theme
            ) {
                onItemTapped.invoke(Item.SelectTabs)
            }.apply { visible = shouldShowSelectTabs },

            SimpleBrowserMenuItem(
                context.getString(R.string.tab_tray_menu_item_share),
                textColorResource = R.color.primary_text_normal_theme
            ) {
                context.components.analytics.metrics.track(Event.TabsTrayShareAllTabsPressed)
                onItemTapped.invoke(Item.ShareAllTabs)
            }.apply { visible = { shouldShowShareAllTabs() && hasOpenTabs() } },

            SimpleBrowserMenuItem(
                context.getString(R.string.tab_tray_menu_tab_settings),
                textColorResource = R.color.primary_text_normal_theme
            ) {
                onItemTapped.invoke(Item.OpenTabSettings)
            },

            SimpleBrowserMenuItem(
                context.getString(R.string.tab_tray_menu_recently_closed),
                textColorResource = R.color.primary_text_normal_theme
            ) {
                onItemTapped.invoke(Item.OpenRecentlyClosed)
            },

            SimpleBrowserMenuItem(
                context.getString(R.string.tab_tray_menu_item_close),
                textColorResource = R.color.primary_text_normal_theme
            ) {
                context.components.analytics.metrics.track(Event.TabsTrayCloseAllTabsPressed)
                onItemTapped.invoke(Item.CloseAllTabs)
            }.apply { visible = hasOpenTabs }
        )
    }
}
