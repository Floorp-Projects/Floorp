/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import com.google.android.material.tabs.TabLayout
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.tabstray.TrayPagerAdapter.Companion.POSITION_NORMAL_TABS
import org.mozilla.fenix.tabstray.TrayPagerAdapter.Companion.POSITION_PRIVATE_TABS
import org.mozilla.fenix.tabstray.TrayPagerAdapter.Companion.POSITION_SYNCED_TABS

@RunWith(FenixRobolectricTestRunner::class)
class TabLayoutMediatorTest {

    @Test
    fun `page to normal tab position when selected tab is also normal`() {
        val store = createStore("123")
        val tabsTrayStore: TabsTrayStore = mockk(relaxed = true)
        val tabLayout: TabLayout = mockk(relaxed = true)
        val tab: TabLayout.Tab = mockk(relaxed = true)
        val mediator = TabLayoutMediator(tabLayout, mockk(relaxed = true), store, tabsTrayStore, mockk())

        every { tabLayout.getTabAt(POSITION_NORMAL_TABS) }.answers { tab }

        mediator.selectActivePage()

        verify { tab.select() }
        verify { tabsTrayStore.dispatch(TabsTrayAction.PageSelected(Page.positionToPage(POSITION_NORMAL_TABS))) }
    }

    @Test
    fun `page to private tab position when selected tab is also private`() {
        val store = createStore("456")
        val tabsTrayStore: TabsTrayStore = mockk(relaxed = true)
        val tabLayout: TabLayout = mockk(relaxed = true)
        val tab: TabLayout.Tab = mockk(relaxed = true)
        val mediator = TabLayoutMediator(tabLayout, mockk(relaxed = true), store, tabsTrayStore, mockk())

        every { tabLayout.getTabAt(POSITION_PRIVATE_TABS) }.answers { tab }

        mediator.selectActivePage()

        verify { tab.select() }
        verify { tabsTrayStore.dispatch(TabsTrayAction.PageSelected(Page.positionToPage(POSITION_PRIVATE_TABS))) }
    }

    @Test
    fun `selectTabAtPosition will dispatch the correct TabsTrayStore action`() {
        val store = createStore("456")
        val tabsTrayStore: TabsTrayStore = mockk(relaxed = true)
        val tabLayout: TabLayout = mockk(relaxed = true)
        val tab: TabLayout.Tab = mockk(relaxed = true)
        val mediator = TabLayoutMediator(tabLayout, mockk(relaxed = true), store, tabsTrayStore, mockk())

        every { tabLayout.getTabAt(POSITION_NORMAL_TABS) }.answers { tab }
        every { tabLayout.getTabAt(POSITION_PRIVATE_TABS) }.answers { tab }
        every { tabLayout.getTabAt(POSITION_SYNCED_TABS) }.answers { tab }

        mediator.selectTabAtPosition(POSITION_NORMAL_TABS)
        verify { tab.select() }
        verify { tabsTrayStore.dispatch(TabsTrayAction.PageSelected(Page.positionToPage(POSITION_NORMAL_TABS))) }

        mediator.selectTabAtPosition(POSITION_PRIVATE_TABS)
        verify { tab.select() }
        verify { tabsTrayStore.dispatch(TabsTrayAction.PageSelected(Page.positionToPage(POSITION_PRIVATE_TABS))) }

        mediator.selectTabAtPosition(POSITION_SYNCED_TABS)
        verify { tab.select() }
        verify { tabsTrayStore.dispatch(TabsTrayAction.PageSelected(Page.positionToPage(POSITION_SYNCED_TABS))) }
    }

    @Test
    fun `lifecycle methods adds and removes observer`() {
        val store = createStore("456")
        val tabsTrayStore: TabsTrayStore = mockk(relaxed = true)
        val tabLayout: TabLayout = mockk(relaxed = true)
        val mediator = TabLayoutMediator(tabLayout, mockk(relaxed = true), store, tabsTrayStore, mockk())

        mediator.start()

        verify { tabLayout.addOnTabSelectedListener(any()) }
        verify { tabsTrayStore.dispatch(TabsTrayAction.PageSelected(Page.positionToPage(POSITION_PRIVATE_TABS))) }

        mediator.stop()

        verify { tabLayout.removeOnTabSelectedListener(any()) }
    }

    private fun createStore(selectedId: String) = BrowserStore(
        initialState = BrowserState(
            tabs = listOf(
                TabSessionState(
                    id = "123",
                    content = ContentState(
                        private = false,
                        url = "https://firefox.com"
                    )
                ),
                TabSessionState(
                    id = "456",
                    content = ContentState(
                        private = true,
                        url = "https://mozilla.org"
                    )
                )
            ),
            selectedTabId = selectedId
        )
    )
}
