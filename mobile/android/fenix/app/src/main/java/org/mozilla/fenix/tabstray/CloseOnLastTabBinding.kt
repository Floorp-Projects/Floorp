/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.distinctUntilChangedBy
import kotlinx.coroutines.flow.drop
import kotlinx.coroutines.flow.map
import mozilla.components.browser.state.selector.normalTabs
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.helpers.AbstractBinding

/**
 * A binding that closes the tabs tray when the last tab is closed.
 */
@OptIn(ExperimentalCoroutinesApi::class)
class CloseOnLastTabBinding(
    browserStore: BrowserStore,
    private val tabsTrayStore: TabsTrayStore,
    private val navigationInteractor: NavigationInteractor,
) : AbstractBinding<BrowserState>(browserStore) {
    override suspend fun onState(flow: Flow<BrowserState>) {
        flow.map { it }
            // Ignore the initial state; we don't want to close immediately.
            .drop(1)
            .distinctUntilChangedBy { it.tabs }
            .collect { state ->
                val selectedPage = tabsTrayStore.state.selectedPage
                val tabs = when (selectedPage) {
                    Page.NormalTabs -> {
                        state.normalTabs
                    }
                    Page.PrivateTabs -> {
                        state.privateTabs
                    }
                    else -> {
                        // Do nothing if we're on any other non-browser page.
                        null
                    }
                }
                if (tabs?.isEmpty() == true) {
                    navigationInteractor.onCloseAllTabsClicked(selectedPage == Page.PrivateTabs)
                }
            }
    }
}
