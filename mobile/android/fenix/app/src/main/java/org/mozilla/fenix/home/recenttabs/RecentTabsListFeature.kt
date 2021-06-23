/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recenttabs

import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.collect
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.helpers.AbstractBinding
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifAnyChanged
import org.mozilla.fenix.ext.asRecentTabs
import org.mozilla.fenix.ext.lastOpenedNormalTab
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
        flow
            // Listen for changes regarding the currently selected tab and the in progress media tab
            // and also for changes (close, undo) in normal tabs that could involve these.
            .ifAnyChanged {
                val lastOpenedNormalTab = it.lastOpenedNormalTab
                arrayOf(
                    lastOpenedNormalTab?.id,
                    lastOpenedNormalTab?.content?.title,
                    lastOpenedNormalTab?.content?.icon,
                    it.normalTabs
                )
            }
            .collect {
                homeStore.dispatch(HomeFragmentAction.RecentTabsChange(browserStore.state.asRecentTabs()))
            }
    }
}
