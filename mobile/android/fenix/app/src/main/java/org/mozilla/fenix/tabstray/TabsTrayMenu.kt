/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import android.content.Context
import com.google.android.material.tabs.TabLayout
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.menu.item.SimpleBrowserMenuItem
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.store.BrowserStore
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.fenix.GleanMetrics.TabsTray
import org.mozilla.fenix.R
import org.mozilla.fenix.tabstray.ext.isNormalModeSelected
import org.mozilla.fenix.tabstray.ext.isPrivateModeSelected
import org.mozilla.fenix.tabstray.ext.isSyncedModeSelected

class TabsTrayMenu(
    private val context: Context,
    browserStore: BrowserStore,
    private val tabLayout: TabLayout,
    private val onItemTapped: (Item) -> Unit = {},
) {

    private val checkOpenTabs =
        when {
            tabLayout.isNormalModeSelected() ->
                browserStore.state.normalTabs.isNotEmpty()
            tabLayout.isPrivateModeSelected() ->
                browserStore.state.privateTabs.isNotEmpty()
            else ->
                false
        }

    private val shouldShowSelectOrShare = { tabLayout.isNormalModeSelected() && checkOpenTabs }
    private val shouldShowTabSetting = { !tabLayout.isSyncedModeSelected() }
    private val shouldShowAccountSetting = { tabLayout.isSyncedModeSelected() }

    sealed class Item {
        object ShareAllTabs : Item()
        object OpenAccountSettings : Item()
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
                textColorResource = R.color.fx_mobile_text_color_primary,
            ) {
                onItemTapped.invoke(Item.SelectTabs)
            }.apply { visible = shouldShowSelectOrShare },

            SimpleBrowserMenuItem(
                context.getString(R.string.tab_tray_menu_item_share),
                textColorResource = R.color.fx_mobile_text_color_primary,
            ) {
                TabsTray.shareAllTabs.record(NoExtras())
                onItemTapped.invoke(Item.ShareAllTabs)
            }.apply { visible = shouldShowSelectOrShare },

            SimpleBrowserMenuItem(
                context.getString(R.string.tab_tray_menu_account_settings),
                textColorResource = R.color.fx_mobile_text_color_primary,
            ) {
                onItemTapped.invoke(Item.OpenAccountSettings)
            }.apply { visible = shouldShowAccountSetting },

            SimpleBrowserMenuItem(
                context.getString(R.string.tab_tray_menu_tab_settings),
                textColorResource = R.color.fx_mobile_text_color_primary,
            ) {
                onItemTapped.invoke(Item.OpenTabSettings)
            }.apply { visible = shouldShowTabSetting },

            SimpleBrowserMenuItem(
                context.getString(R.string.tab_tray_menu_recently_closed),
                textColorResource = R.color.fx_mobile_text_color_primary,
            ) {
                onItemTapped.invoke(Item.OpenRecentlyClosed)
            },

            SimpleBrowserMenuItem(
                context.getString(R.string.tab_tray_menu_item_close),
                textColorResource = R.color.fx_mobile_text_color_primary,
            ) {
                TabsTray.closeAllTabs.record(NoExtras())
                onItemTapped.invoke(Item.CloseAllTabs)
            }.apply { visible = { checkOpenTabs } },
        )
    }
}
