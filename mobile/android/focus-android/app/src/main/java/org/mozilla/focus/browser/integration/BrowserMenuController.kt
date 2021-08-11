/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.browser.integration

import mozilla.components.feature.session.SessionUseCases
import org.mozilla.focus.menu.ToolbarMenu
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.state.AppStore
import org.mozilla.focus.state.Screen

@Suppress("LongParameterList")
class BrowserMenuController(
    private val sessionUseCases: SessionUseCases,
    private val appStore: AppStore,
    private val currentTabId: String,
    private val shareCallback: () -> Unit,
    private val requestDesktopCallback: (isChecked: Boolean) -> Unit,
    private val addToHomeScreenCallback: () -> Unit,
    private val showFindInPageCallback: () -> Unit,
    private val openInCallback: () -> Unit
) {

    fun handleMenuInteraction(item: ToolbarMenu.Item) {
        when (item) {
            is ToolbarMenu.Item.Back -> sessionUseCases.goBack(currentTabId)
            is ToolbarMenu.Item.Forward -> sessionUseCases.goForward(currentTabId)
            is ToolbarMenu.Item.Reload -> sessionUseCases.reload(currentTabId)
            is ToolbarMenu.Item.Stop -> sessionUseCases.stopLoading(currentTabId)
            is ToolbarMenu.Item.Share -> shareCallback()
            is ToolbarMenu.Item.FindInPage -> showFindInPageCallback()
            is ToolbarMenu.Item.RequestDesktop -> requestDesktopCallback(item.isChecked)
            is ToolbarMenu.Item.AddToHomeScreen -> addToHomeScreenCallback()
            is ToolbarMenu.Item.OpenInApp -> openInCallback()
            is ToolbarMenu.Item.Settings -> appStore.dispatch(AppAction.OpenSettings(page = Screen.Settings.Page.Start))
        }
    }
}
