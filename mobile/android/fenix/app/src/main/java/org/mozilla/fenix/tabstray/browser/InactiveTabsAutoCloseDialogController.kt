/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import androidx.annotation.VisibleForTesting
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.tabstray.TabsTray
import mozilla.components.feature.tabs.ext.toTabs
import org.mozilla.fenix.utils.Settings

class InactiveTabsAutoCloseDialogController(
    private val browserStore: BrowserStore,
    private val settings: Settings,
    private val tabFilter: (TabSessionState) -> Boolean,
    private val tray: TabsTray
) {
    /**
     * Dismiss the auto-close dialog.
     */
    fun close() {
        settings.hasInactiveTabsAutoCloseDialogBeenDismissed = true
        refeshInactiveTabsSecion()
    }

    /**
     * Enable the auto-close feature with the after a month setting.
     */
    fun enableAutoClosed() {
        settings.closeTabsAfterOneMonth = true
        settings.closeTabsAfterOneWeek = false
        settings.closeTabsAfterOneDay = false
        settings.manuallyCloseTabs = false
        refeshInactiveTabsSecion()
    }

    @VisibleForTesting
    internal fun refeshInactiveTabsSecion() {
        val tabs = browserStore.state.toTabs { tabFilter.invoke(it) }
        tray.updateTabs(tabs)
    }
}
