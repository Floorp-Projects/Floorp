/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.recentlyclosed

import androidx.navigation.NavController
import androidx.navigation.NavOptions
import io.mockk.Runs
import io.mockk.clearMocks
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.verify
import io.mockk.verifyAll
import kotlinx.coroutines.test.TestCoroutineDispatcher
import mozilla.components.browser.state.action.RecentlyClosedAction
import mozilla.components.browser.state.state.recover.RecoverableTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.prompt.ShareData
import mozilla.components.feature.tabs.TabsUseCases
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.ext.directionsEq
import org.mozilla.fenix.ext.optionsEq
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class DefaultRecentlyClosedControllerTest {
    private val dispatcher = TestCoroutineDispatcher()
    private val navController: NavController = mockk(relaxed = true)
    private val activity: HomeActivity = mockk(relaxed = true)
    private val browserStore: BrowserStore = mockk(relaxed = true)
    private val recentlyClosedStore: RecentlyClosedFragmentStore = mockk(relaxed = true)
    private val tabsUseCases: TabsUseCases = mockk(relaxed = true)
    private val metrics: MetricController = mockk(relaxed = true)

    @Before
    fun setUp() {
        every { tabsUseCases.restore.invoke(any(), true) } just Runs
    }

    @After
    fun tearDown() {
        dispatcher.cleanupTestCoroutines()
    }

    @Test
    fun handleOpen() {
        val item: RecoverableTab = mockk(relaxed = true)

        var actualtab: RecoverableTab? = null
        var actualBrowsingMode: BrowsingMode? = null

        val controller = createController(
            openToBrowser = { tab, browsingMode ->
                actualtab = tab
                actualBrowsingMode = browsingMode
            }
        )

        controller.handleOpen(item, BrowsingMode.Private)

        assertEquals(item, actualtab)
        assertEquals(actualBrowsingMode, BrowsingMode.Private)

        actualtab = null
        actualBrowsingMode = null

        controller.handleOpen(item, BrowsingMode.Normal)

        assertEquals(item, actualtab)
        assertEquals(actualBrowsingMode, BrowsingMode.Normal)
    }

    @Test
    fun `open multiple tabs`() {
        val tabs = createFakeTabList(2)

        val actualTabs = mutableListOf<RecoverableTab>()
        val actualBrowsingModes = mutableListOf<BrowsingMode?>()

        val controller = createController(
            openToBrowser = { tab, mode ->
                actualTabs.add(tab)
                actualBrowsingModes.add(mode)
            }
        )

        controller.handleOpen(tabs.toSet(), BrowsingMode.Normal)

        assertEquals(2, actualTabs.size)
        assertEquals(tabs[0], actualTabs[0])
        assertEquals(tabs[1], actualTabs[1])
        assertEquals(BrowsingMode.Normal, actualBrowsingModes[0])
        assertEquals(BrowsingMode.Normal, actualBrowsingModes[1])
        verifyAll { metrics.track(Event.RecentlyClosedTabsMenuOpenInNormalTab) }
        clearMocks(metrics)

        actualTabs.clear()
        actualBrowsingModes.clear()

        controller.handleOpen(tabs.toSet(), BrowsingMode.Private)

        assertEquals(2, actualTabs.size)
        assertEquals(tabs[0], actualTabs[0])
        assertEquals(tabs[1], actualTabs[1])
        assertEquals(BrowsingMode.Private, actualBrowsingModes[0])
        assertEquals(BrowsingMode.Private, actualBrowsingModes[1])
        verifyAll { metrics.track(Event.RecentlyClosedTabsMenuOpenInPrivateTab) }
    }

    @Test
    fun `handle selecting first tab`() {
        val selectedTab = createFakeTab()
        every { recentlyClosedStore.state.selectedTabs } returns emptySet()

        createController().handleSelect(selectedTab)

        verify { recentlyClosedStore.dispatch(RecentlyClosedFragmentAction.Select(selectedTab)) }
        verify { metrics.track(Event.RecentlyClosedTabsEnterMultiselect) }
    }

    @Test
    fun `handle selecting a successive tab`() {
        val selectedTab = createFakeTab()
        every { recentlyClosedStore.state.selectedTabs } returns setOf(mockk())

        createController().handleSelect(selectedTab)

        verify { recentlyClosedStore.dispatch(RecentlyClosedFragmentAction.Select(selectedTab)) }
        verify(exactly = 0) { metrics.track(Event.RecentlyClosedTabsEnterMultiselect) }
    }

    @Test
    fun `handle deselect last tab`() {
        val deselectedTab = createFakeTab()
        every { recentlyClosedStore.state.selectedTabs } returns setOf(deselectedTab)

        createController().handleDeselect(deselectedTab)

        verify { recentlyClosedStore.dispatch(RecentlyClosedFragmentAction.Deselect(deselectedTab)) }
        verify { metrics.track(Event.RecentlyClosedTabsExitMultiselect) }
    }

    @Test
    fun `handle deselect a tab from others still selected`() {
        val deselectedTab = createFakeTab()
        every { recentlyClosedStore.state.selectedTabs } returns setOf(deselectedTab, mockk())

        createController().handleDeselect(deselectedTab)

        verify { recentlyClosedStore.dispatch(RecentlyClosedFragmentAction.Deselect(deselectedTab)) }
        verify(exactly = 0) { metrics.track(Event.RecentlyClosedTabsExitMultiselect) }
    }

    @Test
    fun handleDelete() {
        val item: RecoverableTab = mockk(relaxed = true)

        createController().handleDelete(item)

        verify {
            browserStore.dispatch(RecentlyClosedAction.RemoveClosedTabAction(item))
        }
        verify { metrics.track(Event.RecentlyClosedTabsDeleteTab) }
    }

    @Test
    fun `delete multiple tabs`() {
        val tabs = createFakeTabList(2)

        createController().handleDelete(tabs.toSet())

        verify {
            browserStore.dispatch(RecentlyClosedAction.RemoveClosedTabAction(tabs[0]))
            browserStore.dispatch(RecentlyClosedAction.RemoveClosedTabAction(tabs[1]))
        }
        verify { metrics.track(Event.RecentlyClosedTabsMenuDelete) }
    }

    @Test
    fun handleNavigateToHistory() {
        createController().handleNavigateToHistory()

        verify {
            navController.navigate(
                directionsEq(
                    RecentlyClosedFragmentDirections.actionGlobalHistoryFragment()
                ),
                optionsEq(NavOptions.Builder().setPopUpTo(R.id.historyFragment, true).build())
            )
        }
        verify { metrics.track(Event.RecentlyClosedTabsShowFullHistory) }
    }

    @Test
    fun `share multiple tabs`() {
        val tabs = createFakeTabList(2)

        createController().handleShare(tabs.toSet())

        verify {
            val data = arrayOf(
                ShareData(title = tabs[0].title, url = tabs[0].url),
                ShareData(title = tabs[1].title, url = tabs[1].url)
            )
            navController.navigate(
                directionsEq(RecentlyClosedFragmentDirections.actionGlobalShareFragment(data))
            )
        }
        verify { metrics.track(Event.RecentlyClosedTabsMenuShare) }
    }

    @Test
    fun handleRestore() {
        val item: RecoverableTab = mockk(relaxed = true)

        createController().handleRestore(item)

        dispatcher.advanceUntilIdle()

        verify { tabsUseCases.restore.invoke(item, true) }
        verify { metrics.track(Event.RecentlyClosedTabsOpenTab) }
    }

    @Test
    fun `exist multi-select mode when back is pressed`() {
        every { recentlyClosedStore.state.selectedTabs } returns createFakeTabList(3).toSet()

        createController().handleBackPressed()

        verify { recentlyClosedStore.dispatch(RecentlyClosedFragmentAction.DeselectAll) }
        verifyAll { metrics.track(Event.RecentlyClosedTabsExitMultiselect) }
    }

    @Test
    fun `report closing the fragment when back is pressed`() {
        every { recentlyClosedStore.state.selectedTabs } returns emptySet()

        createController().handleBackPressed()

        verify(exactly = 0) { recentlyClosedStore.dispatch(RecentlyClosedFragmentAction.DeselectAll) }
        verifyAll { metrics.track(Event.RecentlyClosedTabsClosed) }
    }

    private fun createController(
        openToBrowser: (RecoverableTab, BrowsingMode?) -> Unit = { _, _ -> }
    ): RecentlyClosedController {
        return DefaultRecentlyClosedController(
            navController,
            browserStore,
            recentlyClosedStore,
            tabsUseCases,
            activity,
            metrics,
            openToBrowser
        )
    }

    private fun createFakeTab(id: String = "FakeId", url: String = "www.fake.com"): RecoverableTab =
        RecoverableTab(id, url)

    private fun createFakeTabList(size: Int): List<RecoverableTab> {
        val fakeTabs = mutableListOf<RecoverableTab>()
        for (i in 0 until size) {
            fakeTabs.add(createFakeTab(id = "FakeId$i"))
        }

        return fakeTabs
    }
}
