/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import androidx.annotation.VisibleForTesting
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.tabstray.TabsTray
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.utils.Settings

class InactiveTabsController(
    private val browserStore: BrowserStore,
    private val appStore: AppStore,
    private val tabFilter: (TabSessionState) -> Boolean,
    private val tray: TabsTray,
    private val metrics: MetricController,
    private val settings: Settings
) {
    /**
     * Updates the inactive card to be expanded to display all the tabs, or collapsed with only
     * the title showing.
     */
    fun updateCardExpansion(isExpanded: Boolean) {
        appStore.dispatch(AppAction.UpdateInactiveExpanded(isExpanded))

        metrics.track(
            when (isExpanded) {
                true -> Event.TabsTrayInactiveTabsExpanded
                false -> Event.TabsTrayInactiveTabsCollapsed
            }
        )

        refreshInactiveTabsSection()
    }

    /**
     * Dismiss the auto-close dialog.
     */
    fun close() {
        markDialogAsShown()
        refreshInactiveTabsSection()
        metrics.track(Event.TabsTrayAutoCloseDialogDismissed)
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
        refreshInactiveTabsSection()
        metrics.track(Event.TabsTrayAutoCloseDialogTurnOnClicked)
    }

    /**
     * Marks the dialog as shown and to not be displayed again.
     */
    private fun markDialogAsShown() {
        settings.hasInactiveTabsAutoCloseDialogBeenDismissed = true
    }

    @VisibleForTesting
    internal fun refreshInactiveTabsSection() {
        val tabs = browserStore.state.tabs.filter(tabFilter)
        tray.updateTabs(tabs, browserStore.state.selectedTabId)
    }
}
