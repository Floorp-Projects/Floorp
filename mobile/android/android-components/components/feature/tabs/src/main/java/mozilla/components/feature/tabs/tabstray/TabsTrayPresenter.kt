/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs.tabstray

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.collect
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabPartition
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.tabstray.TabsTray
import mozilla.components.feature.tabs.ext.toTabList
import mozilla.components.feature.tabs.ext.toTabs
import mozilla.components.lib.state.ext.flowScoped
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged

/**
 * Presenter implementation for a tabs tray implementation in order to update the tabs tray whenever
 * the state of the session manager changes.
 */
class TabsTrayPresenter(
    private val tabsTray: TabsTray,
    private val store: BrowserStore,
    internal var tabsFilter: (TabSessionState) -> Boolean,
    internal var tabPartitionsFilter: (Map<String, TabPartition>) -> TabPartition?,
    private val closeTabsTray: () -> Unit,
) {
    private var scope: CoroutineScope? = null
    private var initialOpen: Boolean = true

    fun start() {
        scope = store.flowScoped { flow -> collect(flow) }
    }

    fun stop() {
        scope?.cancel()
    }

    private suspend fun collect(flow: Flow<BrowserState>) {
        flow.ifChanged { Pair(it.toTabs(tabsFilter), tabPartitionsFilter(it.tabPartitions)) }
            .collect { state ->
                val (tabs, selectedTabId) = state.toTabList(tabsFilter)
                // Do not invoke the callback on start if this is the initial state.
                if (tabs.isEmpty() && !initialOpen) {
                    closeTabsTray.invoke()
                }

                tabsTray.updateTabs(tabs, tabPartitionsFilter(state.tabPartitions), selectedTabId)

                initialOpen = false
            }
    }
}
