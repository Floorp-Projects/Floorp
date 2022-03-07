/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import androidx.annotation.VisibleForTesting
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.tabstray.TabsTray
import mozilla.telemetry.glean.private.NoExtras
import org.mozilla.fenix.utils.Settings
import org.mozilla.fenix.GleanMetrics.TabsTray as TabsTrayMetrics

class InactiveTabsAutoCloseDialogController(
    private val browserStore: BrowserStore,
    private val settings: Settings,
    private val tabFilter: (TabSessionState) -> Boolean,
    private val tray: TabsTray,
) {
    /**
     * Dismiss the auto-close dialog.
     */
    fun close() {
        markDialogAsShown()
        refreshInactiveTabsSection()
        TabsTrayMetrics.autoCloseSeen.record(NoExtras())

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
        refreshInactiveTabsSection()
        TabsTrayMetrics.autoCloseTurnOnClicked.record(NoExtras())
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
        tray.updateTabs(tabs, null, browserStore.state.selectedTabId)
    }
}
