/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import android.content.Context
import androidx.annotation.VisibleForTesting
import com.google.android.material.tabs.TabLayout
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.state.store.BrowserStore

/**
 * A wrapper class that building the tabs tray menu that handles item clicks.
 */
class MenuIntegration(
    @get:VisibleForTesting internal val context: Context,
    @get:VisibleForTesting internal val browserStore: BrowserStore,
    @get:VisibleForTesting internal val tabsTrayStore: TabsTrayStore,
    @get:VisibleForTesting internal val tabLayout: TabLayout,
    @get:VisibleForTesting internal val navigationInteractor: NavigationInteractor,
) {
    private val tabsTrayItemMenu by lazy {
        TabsTrayMenu(
            context = context,
            browserStore = browserStore,
            tabLayout = tabLayout,
            onItemTapped = ::handleMenuClicked,
        )
    }

    private val isPrivateMode: Boolean
        get() = tabsTrayStore.state.selectedPage == Page.PrivateTabs

    /**
     * Builds the internal menu items list. See [BrowserMenuBuilder.build].
     */
    fun build() = tabsTrayItemMenu.menuBuilder.build(context)

    @VisibleForTesting
    internal fun handleMenuClicked(item: TabsTrayMenu.Item) {
        when (item) {
            is TabsTrayMenu.Item.ShareAllTabs ->
                navigationInteractor.onShareTabsOfTypeClicked(isPrivateMode)
            is TabsTrayMenu.Item.OpenAccountSettings ->
                navigationInteractor.onAccountSettingsClicked()
            is TabsTrayMenu.Item.OpenTabSettings ->
                navigationInteractor.onTabSettingsClicked()
            is TabsTrayMenu.Item.CloseAllTabs ->
                navigationInteractor.onCloseAllTabsClicked(isPrivateMode)
            is TabsTrayMenu.Item.OpenRecentlyClosed ->
                navigationInteractor.onOpenRecentlyClosedClicked()
            is TabsTrayMenu.Item.SelectTabs -> {
                tabsTrayStore.dispatch(TabsTrayAction.EnterSelectMode)
            }
        }
    }
}
