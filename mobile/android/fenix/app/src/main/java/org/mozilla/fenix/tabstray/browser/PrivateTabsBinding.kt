/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.map
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.tabstray.TabsTray
import mozilla.components.lib.state.helpers.AbstractBinding
import org.mozilla.fenix.tabstray.TabsTrayState
import org.mozilla.fenix.tabstray.TabsTrayStore

/**
 * A private tabs observer that updates the provided [TabsTray].
 */
class PrivateTabsBinding(
    store: TabsTrayStore,
    private val browserStore: BrowserStore,
    private val tray: TabsTray,
) : AbstractBinding<TabsTrayState>(store) {
    override suspend fun onState(flow: Flow<TabsTrayState>) {
        flow.map { it.privateTabs }
            .distinctUntilChanged()
            .collect {
                // Getting the selectedTabId from the BrowserStore at a different time might lead to a race.
                tray.updateTabs(it, null, browserStore.state.selectedTabId)
            }
    }
}
