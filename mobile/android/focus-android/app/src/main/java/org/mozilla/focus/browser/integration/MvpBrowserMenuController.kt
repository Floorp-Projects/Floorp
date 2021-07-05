/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.browser.integration

import mozilla.components.browser.state.selector.findCustomTabOrSelectedTab
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.session.SessionUseCases
import org.mozilla.focus.menu.ToolbarMenu
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.state.AppStore
import org.mozilla.focus.state.Screen
import org.mozilla.focus.telemetry.TelemetryWrapper

class MvpBrowserMenuController(
    private val sessionUseCases: SessionUseCases,
    private val appStore: AppStore,
    private val store: BrowserStore,
    private val findInPageIntegration: FindInPageIntegration?,
    private val shareCallback: () -> Unit,
    private val requestDesktopCallback: (isChecked: Boolean) -> Unit,
    private val addToHomeScreenCallback: (url: String, title: String) -> Unit,
    private val openInCallback: () -> Unit,
) {

    val tab: SessionState
        get() = store.state.findCustomTabOrSelectedTab()
        // Workaround for tab not existing temporarily.
            ?: createTab("about:blank")

    fun handleMenuInteraction(item: ToolbarMenu.Item) {
        when (item) {
            is ToolbarMenu.Item.Back -> onBackPressed()
            is ToolbarMenu.Item.Forward -> onForwardPressed()
            is ToolbarMenu.Item.Reload -> onReloadPressed()
            is ToolbarMenu.Item.Stop -> onStopPressed()
            is ToolbarMenu.Item.Share -> onSharePressed()
            is ToolbarMenu.Item.FindInPage -> onFindInPagePressed()
            is ToolbarMenu.Item.RequestDesktop -> onRequestDesktopPressed(item.isChecked)
            is ToolbarMenu.Item.AddToHomeScreen -> onAddToHomeScreenPressed()
            is ToolbarMenu.Item.OpenInApp -> onOpenInPressed()
            is ToolbarMenu.Item.Settings -> onSettingsPressed()
        }
    }

    private fun onBackPressed() {
        sessionUseCases.goBack(tab.id)
    }

    private fun onForwardPressed() {
        sessionUseCases.goForward(tab.id)
    }

    private fun onReloadPressed() {
        sessionUseCases.reload(tab.id)
    }

    private fun onStopPressed() {
        sessionUseCases.stopLoading(tab.id)
    }

    private fun onSharePressed() {
        shareCallback()
    }

    private fun onFindInPagePressed() {
        findInPageIntegration?.show(tab)
        TelemetryWrapper.findInPageMenuEvent()
    }

    private fun onRequestDesktopPressed(checked: Boolean) {
        requestDesktopCallback(checked)
    }

    private fun onAddToHomeScreenPressed() {
        addToHomeScreenCallback(tab.content.url, tab.content.title)
    }

    private fun onOpenInPressed() {
        openInCallback()
    }

    private fun onSettingsPressed() {
        appStore.dispatch(AppAction.OpenSettings(page = Screen.Settings.Page.Start))
    }
}
