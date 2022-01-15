/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.recentlyclosed

import androidx.navigation.NavController
import androidx.navigation.NavOptions
import io.mockk.mockk
import io.mockk.coEvery
import io.mockk.verifyAll
import io.mockk.clearMocks
import io.mockk.every
import io.mockk.verify
import io.mockk.coVerify
import io.mockk.just
import io.mockk.Runs
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.runBlocking
import mozilla.components.browser.state.action.RecentlyClosedAction
import mozilla.components.browser.state.state.recover.TabState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.prompt.ShareData
import mozilla.components.feature.recentlyclosed.RecentlyClosedTabsStorage
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.test.robolectric.testContext
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
    private val navController: NavController = mockk(relaxed = true)
    private val activity: HomeActivity = mockk(relaxed = true)
    private val browserStore: BrowserStore = mockk(relaxed = true)
    private val recentlyClosedStore: RecentlyClosedFragmentStore = mockk(relaxed = true)
    private val tabsUseCases: TabsUseCases = mockk(relaxed = true)
    private val metrics: MetricController = mockk(relaxed = true)

    @Before
    fun setUp() {
        coEvery { tabsUseCases.restore.invoke(any(), any(), true) } just Runs
    }

    @Test
    fun handleOpen() {
        val item: TabState = mockk(relaxed = true)

        var tabUrl: String? = null
        var actualBrowsingMode: BrowsingMode? = null

        val controller = createController(
            openToBrowser = { url, browsingMode ->
                tabUrl = url
                actualBrowsingMode = browsingMode
            }
        )

        controller.handleOpen(item, BrowsingMode.Private)

        assertEquals(item.url, tabUrl)
        assertEquals(actualBrowsingMode, BrowsingMode.Private)

        tabUrl = null
        actualBrowsingMode = null

        controller.handleOpen(item, BrowsingMode.Normal)

        assertEquals(item.url, tabUrl)
        assertEquals(actualBrowsingMode, BrowsingMode.Normal)
    }

    @Test
    fun `open multiple tabs`() {
        val tabs = createFakeTabList(2)

        val tabUrls = mutableListOf<String>()
        val actualBrowsingModes = mutableListOf<BrowsingMode?>()

        val controller = createController(
            openToBrowser = { url, mode ->
                tabUrls.add(url)
                actualBrowsingModes.add(mode)
            }
        )

        controller.handleOpen(tabs.toSet(), BrowsingMode.Normal)

        assertEquals(2, tabUrls.size)
        assertEquals(tabs[0].url, tabUrls[0])
        assertEquals(tabs[1].url, tabUrls[1])
        assertEquals(BrowsingMode.Normal, actualBrowsingModes[0])
        assertEquals(BrowsingMode.Normal, actualBrowsingModes[1])
        verifyAll { metrics.track(Event.RecentlyClosedTabsMenuOpenInNormalTab) }
        clearMocks(metrics)

        tabUrls.clear()
        actualBrowsingModes.clear()

        controller.handleOpen(tabs.toSet(), BrowsingMode.Private)

        assertEquals(2, tabUrls.size)
        assertEquals(tabs[0].url, tabUrls[0])
        assertEquals(tabs[1].url, tabUrls[1])
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
        val item: TabState = mockk(relaxed = true)

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
    fun handleRestore() = runBlocking {
        val item: TabState = mockk(relaxed = true)

        createController(scope = this).handleRestore(item)

        coVerify { tabsUseCases.restore.invoke(eq(item), any(), true) }
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
        scope: CoroutineScope = CoroutineScope(Dispatchers.IO),
        openToBrowser: (String, BrowsingMode?) -> Unit = { _, _ -> },
    ): RecentlyClosedController {
        return DefaultRecentlyClosedController(
            navController,
            browserStore,
            recentlyClosedStore,
            RecentlyClosedTabsStorage(testContext, mockk(), mockk()),
            tabsUseCases,
            activity,
            metrics,
            scope,
            openToBrowser
        )
    }

    private fun createFakeTab(id: String = "FakeId", url: String = "www.fake.com"): TabState =
        TabState(id, url)

    private fun createFakeTabList(size: Int): List<TabState> {
        val fakeTabs = mutableListOf<TabState>()
        for (i in 0 until size) {
            fakeTabs.add(createFakeTab(id = "FakeId$i"))
        }

        return fakeTabs
    }
}
