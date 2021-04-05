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

@RunWith(FenixRobolectricTestRunner::class)
class TabLayoutMediatorTest {

    @Test
    fun `page to normal tab position when selected tab is also normal`() {
        val store = createState("123")
        val tabLayout: TabLayout = mockk(relaxed = true)
        val tab: TabLayout.Tab = mockk(relaxed = true)
        val mediator = TabLayoutMediator(tabLayout, mockk(relaxed = true), store)

        every { tabLayout.getTabAt(POSITION_NORMAL_TABS) }.answers { tab }

        mediator.selectActivePage()

        verify { tab.select() }
    }

    @Test
    fun `page to private tab position when selected tab is also private`() {
        val store = createState("456")
        val tabLayout: TabLayout = mockk(relaxed = true)
        val tab: TabLayout.Tab = mockk(relaxed = true)
        val mediator = TabLayoutMediator(tabLayout, mockk(relaxed = true), store)

        every { tabLayout.getTabAt(POSITION_PRIVATE_TABS) }.answers { tab }

        mediator.selectActivePage()

        verify { tab.select() }
    }

    @Test
    fun `lifecycle methods adds and removes observer`() {
        val store = createState("456")
        val tabLayout: TabLayout = mockk(relaxed = true)
        val mediator = TabLayoutMediator(tabLayout, mockk(relaxed = true), store)

        mediator.start()

        verify { tabLayout.addOnTabSelectedListener(any()) }

        mediator.stop()

        verify { tabLayout.removeOnTabSelectedListener(any()) }
    }

    private fun createState(selectedId: String) = BrowserStore(
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
