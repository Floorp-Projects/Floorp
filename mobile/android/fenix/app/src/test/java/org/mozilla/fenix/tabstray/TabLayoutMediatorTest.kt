/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray

import androidx.viewpager2.widget.ViewPager2
import com.google.android.material.tabs.TabLayout
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import org.junit.Test
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
import org.mozilla.fenix.tabstray.TrayPagerAdapter.Companion.POSITION_NORMAL_TABS
import org.mozilla.fenix.tabstray.TrayPagerAdapter.Companion.POSITION_PRIVATE_TABS
import org.mozilla.fenix.tabstray.TrayPagerAdapter.Companion.POSITION_SYNCED_TABS

class TabLayoutMediatorTest {
    private val modeManager: BrowsingModeManager = mockk(relaxed = true)
    private val tabsTrayStore: TabsTrayStore = mockk(relaxed = true)
    private val interactor: TabsTrayInteractor = mockk(relaxed = true)
    private val tabLayout: TabLayout = mockk(relaxed = true)
    private val tab: TabLayout.Tab = mockk(relaxed = true)
    private val viewPager: ViewPager2 = mockk(relaxed = true)

    @Test
    fun `page to normal tab position when mode is also normal`() {
        val mediator = TabLayoutMediator(tabLayout, viewPager, interactor, modeManager, tabsTrayStore)

        val mockState: TabsTrayState = mockk()
        every { modeManager.mode }.answers { BrowsingMode.Normal }
        every { tabLayout.getTabAt(POSITION_NORMAL_TABS) }.answers { tab }
        every { tabsTrayStore.state } returns mockState
        every { mockState.selectedPage } returns Page.NormalTabs

        mediator.selectActivePage()

        verify { tab.select() }
        verify { viewPager.setCurrentItem(POSITION_NORMAL_TABS, false) }
        verify { tabsTrayStore.dispatch(TabsTrayAction.PageSelected(Page.positionToPage(POSITION_NORMAL_TABS))) }
    }

    @Test
    fun `page to private tab position when mode is also private`() {
        val mediator = TabLayoutMediator(tabLayout, viewPager, interactor, modeManager, tabsTrayStore)

        every { modeManager.mode }.answers { BrowsingMode.Private }
        every { tabLayout.getTabAt(POSITION_PRIVATE_TABS) }.answers { tab }

        mediator.selectActivePage()

        verify { tab.select() }
        verify { tabsTrayStore.dispatch(TabsTrayAction.PageSelected(Page.positionToPage(POSITION_PRIVATE_TABS))) }
    }

    @Test
    fun `page to synced tabs when selected page is also synced tabs`() {
        val mediator = TabLayoutMediator(tabLayout, viewPager, interactor, modeManager, tabsTrayStore)

        val mockState: TabsTrayState = mockk()
        every { modeManager.mode }.answers { BrowsingMode.Normal }
        every { tabsTrayStore.state } returns mockState
        every { mockState.selectedPage } returns Page.SyncedTabs

        mediator.selectActivePage()

        verify { viewPager.setCurrentItem(POSITION_SYNCED_TABS, false) }
    }

    @Test
    fun `selectTabAtPosition will dispatch the correct TabsTrayStore action`() {
        val mediator = TabLayoutMediator(tabLayout, viewPager, interactor, modeManager, tabsTrayStore)

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
        val mediator = TabLayoutMediator(tabLayout, viewPager, interactor, modeManager, tabsTrayStore)

        every { modeManager.mode }.answers { BrowsingMode.Private }

        mediator.start()

        verify { tabLayout.addOnTabSelectedListener(any()) }
        verify { tabsTrayStore.dispatch(TabsTrayAction.PageSelected(Page.positionToPage(POSITION_PRIVATE_TABS))) }

        mediator.stop()

        verify { tabLayout.removeOnTabSelectedListener(any()) }
    }
}
