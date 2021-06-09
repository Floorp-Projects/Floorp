/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol

import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.map
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.helpers.AbstractBinding
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged
import org.mozilla.fenix.home.HomeFragmentAction
import org.mozilla.fenix.home.HomeFragmentStore

/**
 * View-bound feature that dispatches recent tab changes to the [HomeFragmentStore] when the
 * [BrowserStore] is updated.
 */
@OptIn(ExperimentalCoroutinesApi::class)
class RecentTabsListFeature(
    private val browserStore: BrowserStore,
    private val homeStore: HomeFragmentStore
) : AbstractBinding<BrowserState>(browserStore) {

    override suspend fun onState(flow: Flow<BrowserState>) {
        flow.map { it.selectedTabId }
            .ifChanged()
            .collect { selectedTabId ->
                // Attempt to get the selected normal tab since here may not be a selected tab or
                // the selected tab may be a private tab.
                val selectedTab = browserStore.state.normalTabs.firstOrNull {
                    it.id == selectedTabId
                }
                val recentTabsList = if (selectedTab != null) {
                    listOf(selectedTab)
                } else {
                    emptyList()
                }

                homeStore.dispatch(HomeFragmentAction.RecentTabsChange(recentTabsList))
            }
    }
}
