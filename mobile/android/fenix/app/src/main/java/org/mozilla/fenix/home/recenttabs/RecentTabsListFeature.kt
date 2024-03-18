/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recenttabs

import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.flow.map
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.helpers.AbstractBinding
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.ext.asRecentTabs

/**
 * View-bound feature that dispatches recent tab changes to the [AppStore] when the
 * [BrowserStore] is updated.
 */
@OptIn(ExperimentalCoroutinesApi::class)
class RecentTabsListFeature(
    browserStore: BrowserStore,
    private val appStore: AppStore,
) : AbstractBinding<BrowserState>(browserStore) {

    override suspend fun onState(flow: Flow<BrowserState>) {
        // Listen for changes regarding the currently selected tab and in progress media tab.
        flow
            .map { it.asRecentTabs() }
            .distinctUntilChanged()
            .collect {
                appStore.dispatch(AppAction.RecentTabsChange(it))
            }
    }
}

sealed class RecentTab {
    /**
     * A tab that was recently viewed
     *
     * @property state Recently viewed [TabSessionState]
     */
    data class Tab(val state: TabSessionState) : RecentTab()
}
