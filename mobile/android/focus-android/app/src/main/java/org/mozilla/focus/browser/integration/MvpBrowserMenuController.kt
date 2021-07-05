/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.browser.integration

import androidx.preference.PreferenceManager
import mozilla.components.browser.state.selector.findCustomTabOrSelectedTab
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.createTab
import org.mozilla.focus.R
import org.mozilla.focus.activity.InstallFirefoxActivity
import org.mozilla.focus.ext.requireComponents
import org.mozilla.focus.fragment.BrowserFragment
import org.mozilla.focus.menu.ToolbarMenu
import org.mozilla.focus.open.OpenWithFragment
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.state.Screen
import org.mozilla.focus.telemetry.TelemetryWrapper
import org.mozilla.focus.utils.Browsers

class MvpBrowserMenuController(
    private val fragment: BrowserFragment,
    private val findInPageIntegration: FindInPageIntegration?,
    private val shareCallback: () -> Unit,
    private val addToHomeScreenCallback: (url: String, title: String) -> Unit
) {

    val tab: SessionState
        get() = fragment.requireComponents.store.state.findCustomTabOrSelectedTab()
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
        fragment.requireComponents.sessionUseCases.goBack(tab.id)
    }

    private fun onForwardPressed() {
        fragment.requireComponents.sessionUseCases.goForward(tab.id)
    }

    private fun onReloadPressed() {
        fragment.requireComponents.sessionUseCases.reload(tab.id)
    }

    private fun onStopPressed() {
        fragment.requireComponents.sessionUseCases.stopLoading(tab.id)
    }

    private fun onSharePressed() {
        shareCallback()
    }

    private fun onFindInPagePressed() {
        findInPageIntegration?.show(tab)
        TelemetryWrapper.findInPageMenuEvent()
    }

    private fun onRequestDesktopPressed(checked: Boolean) {
        if (checked) {
            PreferenceManager.getDefaultSharedPreferences(fragment.requireContext()).edit()
                .putBoolean(
                    fragment.requireContext().getString(R.string.has_requested_desktop),
                    true
                ).apply()
        }

        fragment.requireComponents.sessionUseCases.requestDesktopSite(checked, tab.id)
    }

    private fun onAddToHomeScreenPressed() {
        addToHomeScreenCallback(tab.content.url, tab.content.title)
    }

    private fun onOpenInPressed() {
        val browsers = Browsers(fragment.requireContext(), tab.content.url)

        val apps = browsers.installedBrowsers
        val store = if (browsers.hasFirefoxBrandedBrowserInstalled())
            null
        else
            InstallFirefoxActivity.resolveAppStore(fragment.requireContext())

        val openWithFragment = OpenWithFragment.newInstance(
            apps,
            tab.content.url,
            store
        )
        @Suppress("DEPRECATION")
        openWithFragment.show(fragment.requireFragmentManager(), OpenWithFragment.FRAGMENT_TAG)

        TelemetryWrapper.openSelectionEvent()
    }

    private fun onSettingsPressed() {
        fragment.requireComponents.appStore.dispatch(
            AppAction.OpenSettings(page = Screen.Settings.Page.Start)
        )
    }
}
