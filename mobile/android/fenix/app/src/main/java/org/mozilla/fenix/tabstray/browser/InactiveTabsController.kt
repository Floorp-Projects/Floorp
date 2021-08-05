/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.tabstray.TabsTray
import mozilla.components.feature.tabs.ext.toTabs

class InactiveTabsController(
    private val browserStore: BrowserStore,
    private val tabFilter: (TabSessionState) -> Boolean,
    private val tray: TabsTray
) {
    /**
     * Updates the inactive card to be expanded to display all the tabs, or collapsed with only
     * the title showing.
     */
    fun updateCardExpansion(isExpanded: Boolean) {
        InactiveTabsState.isExpanded = isExpanded

        val tabs = browserStore.state.toTabs { tabFilter.invoke(it) }

        tray.updateTabs(tabs)
    }
}
