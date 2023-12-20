/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.bindings

import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.collectLatest
import kotlinx.coroutines.flow.distinctUntilChangedBy
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.helpers.AbstractBinding
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction

/**
 * Binding to update the [AppStore] based on changes to [BrowserState].
 */
class BrowserStoreBinding(
    browserStore: BrowserStore,
    private val appStore: AppStore,
) : AbstractBinding<BrowserState>(browserStore) {
    override suspend fun onState(flow: Flow<BrowserState>) {
        // Update the AppStore with the latest selected tab
        flow.distinctUntilChangedBy { it.selectedTabId }
            .collectLatest { state ->
                state.selectedTab?.let { tab ->
                    // Ignore re-emissions due to lifecycle events, or other pieces of state like
                    // [mode] may get overwritten
                    if (appStore.state.selectedTabId != tab.id) {
                        appStore.dispatch(AppAction.SelectedTabChanged(tab))
                    }
                }
            }
    }
}
