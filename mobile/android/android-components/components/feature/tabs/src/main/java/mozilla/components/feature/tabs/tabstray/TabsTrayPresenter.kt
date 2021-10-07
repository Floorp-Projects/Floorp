/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs.tabstray

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.map
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.tabstray.Tabs
import mozilla.components.concept.tabstray.TabsTray
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
    private val closeTabsTray: () -> Unit
) {
    private var tabs: Tabs? = null
    private var scope: CoroutineScope? = null

    fun start() {
        scope = store.flowScoped { flow -> collect(flow) }
    }

    fun stop() {
        scope?.cancel()
    }

    private suspend fun collect(flow: Flow<BrowserState>) {
        flow.map { it.toTabs(tabsFilter) }
            .ifChanged()
            .collect { tabs ->
                // Do not invoke the callback on start if this is the initial state.
                if (tabs.list.isEmpty() && this.tabs != null) {
                    closeTabsTray.invoke()
                }

                tabsTray.updateTabs(tabs)

                this.tabs = tabs
            }
    }
}
