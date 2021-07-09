/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.browser.integration

import androidx.annotation.VisibleForTesting
import mozilla.components.browser.state.selector.findTabOrCustomTabOrSelectedTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.session.SessionUseCases
import org.mozilla.focus.menu.ToolbarMenu
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.state.AppStore
import org.mozilla.focus.state.Screen
import org.mozilla.focus.telemetry.TelemetryWrapper

@Suppress("LongParameterList")
class BrowserMenuController(
    private val sessionUseCases: SessionUseCases,
    private val appStore: AppStore,
    store: BrowserStore,
    private val findInPageIntegration: FindInPageIntegration?,
    private val currentTabId: String,
    private val shareCallback: () -> Unit,
    private val requestDesktopCallback: (isChecked: Boolean) -> Unit,
    private val addToHomeScreenCallback: (url: String, title: String) -> Unit,
    private val openInCallback: () -> Unit
) {

    @VisibleForTesting
    val currentTab = store.state.findTabOrCustomTabOrSelectedTab(currentTabId)

    fun handleMenuInteraction(item: ToolbarMenu.Item) {
        when (item) {
            is ToolbarMenu.Item.Back -> sessionUseCases.goBack(currentTabId)
            is ToolbarMenu.Item.Forward -> sessionUseCases.goForward(currentTabId)
            is ToolbarMenu.Item.Reload -> sessionUseCases.reload(currentTabId)
            is ToolbarMenu.Item.Stop -> sessionUseCases.stopLoading(currentTabId)
            is ToolbarMenu.Item.Share -> shareCallback()
            is ToolbarMenu.Item.FindInPage -> onFindInPagePressed()
            is ToolbarMenu.Item.RequestDesktop -> requestDesktopCallback(item.isChecked)
            is ToolbarMenu.Item.AddToHomeScreen -> currentTab?.let { state ->
                addToHomeScreenCallback(
                    state.content.url,
                    state.content.title
                )
            }
            is ToolbarMenu.Item.OpenInApp -> openInCallback()
            is ToolbarMenu.Item.Settings -> appStore.dispatch(AppAction.OpenSettings(page = Screen.Settings.Page.Start))
        }
    }

    private fun onFindInPagePressed() {
        currentTab?.let { state ->
            findInPageIntegration?.show(state)
            TelemetryWrapper.findInPageMenuEvent()
        }
    }
}
