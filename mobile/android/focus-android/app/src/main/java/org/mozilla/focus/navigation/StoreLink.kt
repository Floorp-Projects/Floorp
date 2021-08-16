/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.navigation

import kotlinx.coroutines.MainScope
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.filter
import kotlinx.coroutines.flow.filterNotNull
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.launch
import mozilla.components.browser.state.selector.privateTabs
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.ext.flow
import mozilla.components.support.ktx.kotlinx.coroutines.flow.ifChanged
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.state.AppStore

/**
 * Helper for subscribing to the [BrowserStore] and updating the [AppStore] for certain state changes.
 */
class StoreLink(
    private val appStore: AppStore,
    private val browserStore: BrowserStore
) {
    fun start() {
        MainScope().also { scope ->
            scope.launch { observeSelectionChanges(browserStore.flow()) }
            scope.launch { observeTabsClosed(browserStore.flow()) }
        }
    }

    private suspend fun observeSelectionChanges(flow: Flow<BrowserState>) {
        flow.map { state -> state.selectedTabId }
            .ifChanged()
            .filterNotNull()
            .collect { tabId -> appStore.dispatch(AppAction.SelectionChanged(tabId)) }
    }

    private suspend fun observeTabsClosed(flow: Flow<BrowserState>) {
        flow.map { state -> state.privateTabs.isEmpty() }
            .ifChanged()
            .filter { isEmpty -> isEmpty }
            .collect {
                appStore.dispatch(AppAction.NoTabs)
            }
    }
}
