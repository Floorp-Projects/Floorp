/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import mozilla.components.browser.state.state.TabSessionState
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppAction.UpdateInactiveExpanded
import org.mozilla.fenix.tabstray.TrayPagerAdapter.Companion.INACTIVE_TABS_FEATURE_NAME
import org.mozilla.fenix.utils.Settings
import org.mozilla.fenix.GleanMetrics.TabsTray as TabsTrayMetrics

/**
 * Default behavior for handling all user interactions with the Inactive Tabs feature.
 *
 * @param appStore [AppStore] used to dispatch any [AppAction].
 * @param settings [Settings] used to update any user preferences.
 * @param browserInteractor [BrowserTrayInteractor] used to respond to interactions with specific inactive tabs.
 */
class InactiveTabsController(
    private val appStore: AppStore,
    private val settings: Settings,
    private val browserInteractor: BrowserTrayInteractor,
) {

    /**
     * Opens the given inactive tab.
     */
    fun openInactiveTab(tab: TabSessionState) {
        TabsTrayMetrics.openInactiveTab.add()
        browserInteractor.onTabSelected(tab, INACTIVE_TABS_FEATURE_NAME)
    }

    /**
     * Closes the given inactive tab.
     */
    fun closeInactiveTab(tab: TabSessionState) {
        TabsTrayMetrics.closeInactiveTab.add()
        browserInteractor.onTabClosed(tab, INACTIVE_TABS_FEATURE_NAME)
    }

    /**
     * Updates the inactive card to be expanded to display all the tabs, or collapsed with only
     * the title showing.
     */
    fun updateCardExpansion(isExpanded: Boolean) {
        appStore.dispatch(UpdateInactiveExpanded(isExpanded))

        when (isExpanded) {
            true -> TabsTrayMetrics.inactiveTabsExpanded.record(NoExtras())
            false -> TabsTrayMetrics.inactiveTabsCollapsed.record(NoExtras())
        }
    }

    /**
     * Dismiss the auto-close dialog.
     */
    fun close() {
        markDialogAsShown()
        TabsTrayMetrics.autoCloseDimissed.record(NoExtras())
    }

    /**
     * Enable the auto-close feature with the after a month setting.
     */
    fun enableAutoClosed() {
        markDialogAsShown()
        settings.closeTabsAfterOneMonth = true
        settings.closeTabsAfterOneWeek = false
        settings.closeTabsAfterOneDay = false
        settings.manuallyCloseTabs = false
        TabsTrayMetrics.autoCloseTurnOnClicked.record(NoExtras())
    }

    /**
     * Marks the dialog as shown and to not be displayed again.
     */
    private fun markDialogAsShown() {
        settings.hasInactiveTabsAutoCloseDialogBeenDismissed = true
    }
}
