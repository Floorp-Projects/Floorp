/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import android.content.Context
import android.view.View
import androidx.annotation.VisibleForTesting
import androidx.cardview.widget.CardView
import androidx.core.content.ContextCompat
import com.google.android.material.tabs.TabLayout
import mozilla.components.browser.menu.BrowserMenu
import mozilla.components.browser.menu.BrowserMenuBuilder
import mozilla.components.browser.state.store.BrowserStore
import org.mozilla.fenix.R

/**
 * A wrapper class that building the tabs tray menu that handles item clicks.
 */
class MenuIntegration(
    private val context: Context,
    private val browserStore: BrowserStore,
    private val tabsTrayStore: TabsTrayStore,
    private val tabLayout: TabLayout,
    private val navigationInteractor: NavigationInteractor
) {
    private val tabsTrayItemMenu by lazy {
        TabsTrayMenu(
            context = context,
            browserStore = browserStore,
            tabLayout = tabLayout,
            onItemTapped = ::handleMenuClicked
        )
    }

    private val isPrivateMode: Boolean
        get() = tabsTrayStore.state.selectedPage == Page.PrivateTabs

    /**
     * Builds the internal menu items list. See [BrowserMenuBuilder.build].
     */
    fun build() = tabsTrayItemMenu.menuBuilder.build(context)

    @VisibleForTesting
    internal fun handleMenuClicked(item: TabsTrayMenu.Item) = when (item) {
        is TabsTrayMenu.Item.ShareAllTabs ->
            navigationInteractor.onShareTabsOfTypeClicked(isPrivateMode)
        is TabsTrayMenu.Item.OpenTabSettings ->
            navigationInteractor.onTabSettingsClicked()
        is TabsTrayMenu.Item.CloseAllTabs ->
            navigationInteractor.onCloseAllTabsClicked(isPrivateMode)
        is TabsTrayMenu.Item.OpenRecentlyClosed ->
            navigationInteractor.onOpenRecentlyClosedClicked()
        is TabsTrayMenu.Item.SelectTabs -> {
            /* TODO implement when mulitiselect call is available */
        }
    }
}

/**
 * Invokes [BrowserMenu.show] and applies the default theme color background.
 */
fun BrowserMenu.showWithTheme(view: View) {
    show(view).also { popupMenu ->
        (popupMenu.contentView as? CardView)?.setCardBackgroundColor(
            ContextCompat.getColor(
                view.context,
                R.color.foundation_normal_theme
            )
        )
    }
}
