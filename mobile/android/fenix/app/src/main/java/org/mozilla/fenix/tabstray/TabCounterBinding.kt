/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.map
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.helpers.AbstractBinding
import mozilla.components.ui.tabcounter.TabCounter

/**
 * Updates the tab counter to the size of [BrowserState.normalTabs].
 */
@OptIn(ExperimentalCoroutinesApi::class)
class TabCounterBinding(
    store: BrowserStore,
    private val counter: TabCounter,
) : AbstractBinding<BrowserState>(store) {

    override suspend fun onState(flow: Flow<BrowserState>) {
        flow.map { it.normalTabs }
            .distinctUntilChanged()
            .collect {
                counter.setCount(it.size)
            }
    }
}
