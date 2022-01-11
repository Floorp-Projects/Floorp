/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import androidx.annotation.VisibleForTesting
import mozilla.components.browser.tabstray.TabsTray
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction.UpdateInactiveExpanded
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.tabstray.TabsTrayStore
import org.mozilla.fenix.utils.Settings

class InactiveTabsController(
    private val tabsTrayStore: TabsTrayStore,
    private val appStore: AppStore,
    private val tray: TabsTray,
    private val metrics: MetricController,
    private val settings: Settings
) {
    /**
     * Updates the inactive card to be expanded to display all the tabs, or collapsed with only
     * the title showing.
     */
    fun updateCardExpansion(isExpanded: Boolean) {
        appStore.dispatch(UpdateInactiveExpanded(isExpanded)).invokeOnCompletion {
            // To avoid racing, we read the list of inactive tabs only after we have updated
            // the expanded state.
            refreshInactiveTabsSection()
        }

        metrics.track(
            when (isExpanded) {
                true -> Event.TabsTrayInactiveTabsExpanded
                false -> Event.TabsTrayInactiveTabsCollapsed
            }
        )
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
        val tabs = tabsTrayStore.state.inactiveTabs
        tray.updateTabs(tabs, null, null)
    }
}
