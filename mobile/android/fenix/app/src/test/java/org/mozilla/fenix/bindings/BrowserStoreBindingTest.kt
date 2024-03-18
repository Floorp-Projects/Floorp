/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.bindings

import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Rule
import org.junit.Test
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppState

class BrowserStoreBindingTest {

    @get:Rule
    val coroutineRule = MainCoroutineRule()

    lateinit var browserStore: BrowserStore
    lateinit var appStore: AppStore

    private val tabId1 = "1"
    private val tabId2 = "2"
    private val tab1 = createTab(url = tabId1, id = tabId1)
    private val tab2 = createTab(url = tabId2, id = tabId2)

    @Test
    fun `WHEN selected tab changes THEN app action dispatched with update`() = runTestOnMain {
        appStore = spy(AppStore())
        browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(tab1, tab2),
                selectedTabId = tabId1,
            ),
        )

        val binding = BrowserStoreBinding(browserStore, appStore)
        binding.start()
        browserStore.dispatch(TabListAction.SelectTabAction(tabId2)).joinBlocking()

        // consume initial state
        verify(appStore).dispatch(AppAction.SelectedTabChanged(tab1))
        // verify response to Browser Store dispatch
        verify(appStore).dispatch(AppAction.SelectedTabChanged(tab2))
    }

    @Test
    fun `GIVEN selected tab id is set WHEN update is observed with same id THEN update is ignored`() {
        appStore = spy(
            AppStore(
                AppState(
                    selectedTabId = tabId2,
                ),
            ),
        )
        browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(tab1, tab2),
                selectedTabId = tabId2,
            ),
        )

        val binding = BrowserStoreBinding(browserStore, appStore)
        binding.start()
        browserStore.dispatch(TabListAction.SelectTabAction(tabId2)).joinBlocking()

        // the selected tab should only be dispatched on initialization
        verify(appStore, never()).dispatch(AppAction.SelectedTabChanged(tab2))
    }
}
