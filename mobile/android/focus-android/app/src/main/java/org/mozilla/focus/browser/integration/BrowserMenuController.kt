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
import org.mozilla.focus.GleanMetrics.BrowserMenu
import org.mozilla.focus.GleanMetrics.CustomTabsToolbar
import org.mozilla.focus.GleanMetrics.Shortcuts
import org.mozilla.focus.ext.titleOrDomain
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
    private val openInBrowser: () -> Unit,
    private val showShortcutAddedSnackBar: () -> Unit,
) {
    @VisibleForTesting
    private val currentTab: SessionState?
        get() = store.state.findTabOrCustomTabOrSelectedTab(currentTabId)

    @VisibleForTesting
    internal var ioScope: CoroutineScope = CoroutineScope(Dispatchers.IO)

    @Suppress("ComplexMethod")
    fun handleMenuInteraction(item: ToolbarMenu.Item) {
        recordBrowserMenuTelemetry(item)

        when (item) {
            is ToolbarMenu.Item.Back, ToolbarMenu.CustomTabItem.Back -> sessionUseCases.goBack(
                currentTabId,
            )
            is ToolbarMenu.Item.Forward, ToolbarMenu.CustomTabItem.Forward -> sessionUseCases.goForward(
                currentTabId,
            )
            is ToolbarMenu.Item.Reload, ToolbarMenu.CustomTabItem.Reload -> {
                sessionUseCases.reload(currentTabId)
            }
            is ToolbarMenu.Item.Stop, ToolbarMenu.CustomTabItem.Stop -> sessionUseCases.stopLoading(
                currentTabId,
            )
            is ToolbarMenu.Item.Share -> shareCallback()
            is ToolbarMenu.Item.FindInPage, ToolbarMenu.CustomTabItem.FindInPage -> showFindInPageCallback()
            is ToolbarMenu.Item.AddToShortcuts -> {
                ioScope.launch {
                    currentTab?.let { state ->
                        topSitesUseCases.addPinnedSites(
                            title = state.content.titleOrDomain,
                            url = state.content.url,
                        )
                    }
                }
                showShortcutAddedSnackBar()
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
            is ToolbarMenu.CustomTabItem.RequestDesktop -> requestDesktopCallback(item.isChecked)
            is ToolbarMenu.Item.AddToHomeScreen, ToolbarMenu.CustomTabItem.AddToHomeScreen -> addToHomeScreenCallback()
            is ToolbarMenu.CustomTabItem.OpenInBrowser -> openInBrowser()
            is ToolbarMenu.Item.OpenInApp, ToolbarMenu.CustomTabItem.OpenInApp -> openInCallback()
            is ToolbarMenu.Item.Settings -> appStore.dispatch(AppAction.OpenSettings(page = Screen.Settings.Page.Start))
        }
    }

    @Suppress("LongMethod")
    @VisibleForTesting
    internal fun recordBrowserMenuTelemetry(item: ToolbarMenu.Item) {
        when (item) {
            is ToolbarMenu.Item.Back -> BrowserMenu.navigationToolbarAction.record(
                BrowserMenu.NavigationToolbarActionExtra("back"),
            )
            is ToolbarMenu.Item.Forward -> BrowserMenu.navigationToolbarAction.record(
                BrowserMenu.NavigationToolbarActionExtra("forward"),
            )
            is ToolbarMenu.Item.Reload -> {
                BrowserMenu.navigationToolbarAction.record(
                    BrowserMenu.NavigationToolbarActionExtra("reload"),
                )
            }
            is ToolbarMenu.Item.Stop -> BrowserMenu.navigationToolbarAction.record(
                BrowserMenu.NavigationToolbarActionExtra("stop"),
            )
            is ToolbarMenu.Item.Share -> BrowserMenu.navigationToolbarAction.record(
                BrowserMenu.NavigationToolbarActionExtra("share"),
            )
            is ToolbarMenu.Item.FindInPage -> BrowserMenu.browserMenuAction.record(
                BrowserMenu.BrowserMenuActionExtra("find_in_page"),
            )
            is ToolbarMenu.Item.AddToShortcuts ->
                Shortcuts.shortcutAddedCounter.add()
            is ToolbarMenu.Item.RemoveFromShortcuts ->
                Shortcuts.shortcutRemovedCounter["removed_from_browser_menu"].add()

            is ToolbarMenu.Item.RequestDesktop -> {
                if (item.isChecked) {
                    BrowserMenu.browserMenuAction.record(
                        BrowserMenu.BrowserMenuActionExtra("desktop_view_on"),
                    )
                } else {
                    BrowserMenu.browserMenuAction.record(
                        BrowserMenu.BrowserMenuActionExtra("desktop_view_off"),
                    )
                }
            }
            is ToolbarMenu.Item.AddToHomeScreen -> BrowserMenu.browserMenuAction.record(
                BrowserMenu.BrowserMenuActionExtra("add_to_home_screen"),
            )

            is ToolbarMenu.Item.OpenInApp -> BrowserMenu.browserMenuAction.record(
                BrowserMenu.BrowserMenuActionExtra("open_in_app"),
            )
            is ToolbarMenu.Item.Settings -> BrowserMenu.browserMenuAction.record(
                BrowserMenu.BrowserMenuActionExtra("settings"),
            )

            // custom tabs
            ToolbarMenu.CustomTabItem.Back -> CustomTabsToolbar.navigationToolbarAction.record(
                CustomTabsToolbar.NavigationToolbarActionExtra("back"),
            )
            ToolbarMenu.CustomTabItem.Forward -> CustomTabsToolbar.navigationToolbarAction.record(
                CustomTabsToolbar.NavigationToolbarActionExtra("forward"),
            )
            ToolbarMenu.CustomTabItem.Stop -> CustomTabsToolbar.navigationToolbarAction.record(
                CustomTabsToolbar.NavigationToolbarActionExtra("stop"),
            )

            ToolbarMenu.CustomTabItem.Reload -> {
                CustomTabsToolbar.navigationToolbarAction.record(
                    CustomTabsToolbar.NavigationToolbarActionExtra("reload"),
                )
            }

            ToolbarMenu.CustomTabItem.AddToHomeScreen -> CustomTabsToolbar.browserMenuAction.record(
                CustomTabsToolbar.BrowserMenuActionExtra("add_to_home_screen"),
            )
            ToolbarMenu.CustomTabItem.OpenInApp -> CustomTabsToolbar.browserMenuAction.record(
                CustomTabsToolbar.BrowserMenuActionExtra("open_in_app"),
            )
            ToolbarMenu.CustomTabItem.OpenInBrowser -> CustomTabsToolbar.browserMenuAction.record(
                CustomTabsToolbar.BrowserMenuActionExtra("open_in_browser"),
            )

            ToolbarMenu.CustomTabItem.FindInPage -> CustomTabsToolbar.browserMenuAction.record(
                CustomTabsToolbar.BrowserMenuActionExtra("find_in_page"),
            )
            is ToolbarMenu.CustomTabItem.RequestDesktop -> {
                if (item.isChecked) {
                    CustomTabsToolbar.browserMenuAction.record(
                        CustomTabsToolbar.BrowserMenuActionExtra("desktop_view_on"),
                    )
                } else {
                    CustomTabsToolbar.browserMenuAction.record(
                        CustomTabsToolbar.BrowserMenuActionExtra("desktop_view_off"),
                    )
                }
            }
        }
    }
}
