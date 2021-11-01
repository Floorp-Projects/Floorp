/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.tabstray.TabsTray
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.components.metrics.Event

class InactiveTabsController(
    private val browserStore: BrowserStore,
    private val appStore: AppStore,
    private val tabFilter: (TabSessionState) -> Boolean,
    private val tray: TabsTray,
    private val metrics: MetricController
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

        val tabs = browserStore.state.tabs.filter(tabFilter)
        tray.updateTabs(tabs, browserStore.state.selectedTabId)
    }
}
