/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.browser.integration

import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.browser.state.selector.findTabOrCustomTabOrSelectedTab
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.session.SessionUseCases
import mozilla.components.feature.top.sites.TopSitesUseCases
import org.mozilla.focus.menu.ToolbarMenu
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.state.AppStore
import org.mozilla.focus.state.Screen

@Suppress("LongParameterList")
class BrowserMenuController(
    private val sessionUseCases: SessionUseCases,
    private val appStore: AppStore,
    private val store: BrowserStore,
    private val topSitesUseCases: TopSitesUseCases,
    private val currentTabId: String,
    private val shareCallback: () -> Unit,
    private val requestDesktopCallback: (isChecked: Boolean) -> Unit,
    private val addToHomeScreenCallback: () -> Unit,
    private val showFindInPageCallback: () -> Unit,
    private val openInCallback: () -> Unit,
    private val openInBrowser: () -> Unit
) {
    @VisibleForTesting
    private val currentTab: SessionState?
        get() = store.state.findTabOrCustomTabOrSelectedTab(currentTabId)

    @VisibleForTesting
    internal var ioScope: CoroutineScope = CoroutineScope(Dispatchers.IO)

    @Suppress("ComplexMethod")
    fun handleMenuInteraction(item: ToolbarMenu.Item) {
        when (item) {
            is ToolbarMenu.Item.Back -> sessionUseCases.goBack(currentTabId)
            is ToolbarMenu.Item.Forward -> sessionUseCases.goForward(currentTabId)
            is ToolbarMenu.Item.Reload -> sessionUseCases.reload(currentTabId)
            is ToolbarMenu.Item.Stop -> sessionUseCases.stopLoading(currentTabId)
            is ToolbarMenu.Item.Share -> shareCallback()
            is ToolbarMenu.Item.FindInPage -> showFindInPageCallback()
            is ToolbarMenu.Item.AddToShortcuts -> {
                ioScope.launch {
                    currentTab?.let { state ->
                        topSitesUseCases.addPinnedSites(
                            title = state.content.title,
                            url = state.content.url
                        )
                    }
                }
            }
            is ToolbarMenu.Item.RemoveFromShortcuts -> {
                ioScope.launch {
                    currentTab?.let { state ->
                        appStore.state.topSites.find { it.url == state.content.url }
                            ?.let { topSite ->
                                topSitesUseCases.removeTopSites(topSite)
                            }
                    }
                }
            }
            is ToolbarMenu.Item.RequestDesktop -> requestDesktopCallback(item.isChecked)
            is ToolbarMenu.Item.AddToHomeScreen -> addToHomeScreenCallback()
            is ToolbarMenu.Item.OpenInBrowser -> openInBrowser()
            is ToolbarMenu.Item.OpenInApp -> openInCallback()
            is ToolbarMenu.Item.Settings -> appStore.dispatch(AppAction.OpenSettings(page = Screen.Settings.Page.Start))
        }
    }
}
