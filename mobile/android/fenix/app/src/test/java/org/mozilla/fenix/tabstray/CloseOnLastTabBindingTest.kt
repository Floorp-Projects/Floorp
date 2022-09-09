/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import io.mockk.Called
import io.mockk.mockk
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Rule
import org.junit.Test

class CloseOnLastTabBindingTest {

    @OptIn(ExperimentalCoroutinesApi::class)
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    @Test
    fun `WHEN the binding starts THEN do nothing`() {
        val browserStore = BrowserStore()
        val tabsTrayStore = TabsTrayStore()
        val interactor = mockk<NavigationInteractor>(relaxed = true)
        val binding = CloseOnLastTabBinding(browserStore, tabsTrayStore, interactor)

        binding.start()

        verify { interactor wasNot Called }
    }

    @Test
    fun `WHEN a tab is closed THEN invoke the interactor`() {
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab(
                        "https://mozilla.org",
                        id = "tab1",
                    ),
                ),
            ),
        )
        val tabsTrayStore = TabsTrayStore()
        val interactor = mockk<NavigationInteractor>(relaxed = true)
        val binding = CloseOnLastTabBinding(browserStore, tabsTrayStore, interactor)

        binding.start()

        browserStore.dispatch(TabListAction.RemoveTabAction("tab1"))

        browserStore.waitUntilIdle()

        verify { interactor.onCloseAllTabsClicked(false) }
    }

    @Test
    fun `WHEN a private tab is closed THEN invoke the interactor`() {
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab(
                        "https://mozilla.org",
                        id = "tab1",
                        private = true,
                    ),
                ),
            ),
        )
        val tabsTrayStore = TabsTrayStore(TabsTrayState(selectedPage = Page.PrivateTabs))
        val interactor = mockk<NavigationInteractor>(relaxed = true)
        val binding = CloseOnLastTabBinding(browserStore, tabsTrayStore, interactor)

        binding.start()

        browserStore.dispatch(TabListAction.RemoveTabAction("tab1"))

        browserStore.waitUntilIdle()

        verify { interactor.onCloseAllTabsClicked(true) }
    }

    @Test
    fun `WHEN on the synced tabs page THEN nothing is invoked`() {
        val browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab(
                        "https://mozilla.org",
                        id = "tab1",
                        private = true,
                    ),
                ),
            ),
        )
        val tabsTrayStore = TabsTrayStore(TabsTrayState(selectedPage = Page.SyncedTabs))
        val interactor = mockk<NavigationInteractor>(relaxed = true)
        val binding = CloseOnLastTabBinding(browserStore, tabsTrayStore, interactor)

        binding.start()

        browserStore.dispatch(TabListAction.RemoveAllTabsAction())

        browserStore.waitUntilIdle()

        verify { interactor wasNot Called }
    }
}
