/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recenttabs

import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.map
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.selector.selectedNormalTab
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.helpers.AbstractBinding
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifAnyChanged
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
        flow.map { it.selectedTab }
            .ifAnyChanged { arrayOf(it?.id, it?.content?.title, it?.content?.icon) }
            .collect { _ ->
                // Attempt to get the selected normal tab or the last accessed normal tab
                // if there is no selected tab or the selected tab is a private one.
                val selectedTab = browserStore.state.selectedNormalTab
                    ?: browserStore.state.normalTabs.maxByOrNull { it.lastAccess }
                val recentTabsList = if (selectedTab != null) {
                    listOf(selectedTab)
                } else {
                    emptyList()
                }

                homeStore.dispatch(HomeFragmentAction.RecentTabsChange(recentTabsList))
            }
    }
}
