/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.recentlyclosed

import androidx.navigation.NavController
import androidx.navigation.NavOptions
import io.mockk.mockk
import io.mockk.coEvery
import io.mockk.every
import io.mockk.verify
import io.mockk.coVerify
import io.mockk.just
import io.mockk.Runs
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.test.runCurrent
import kotlinx.coroutines.test.runTest
import mozilla.components.browser.state.action.RecentlyClosedAction
import mozilla.components.browser.state.state.recover.TabState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.prompt.ShareData
import mozilla.components.feature.recentlyclosed.RecentlyClosedTabsStorage
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.test.robolectric.testContext
import mozilla.telemetry.glean.testing.GleanTestRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.RecentlyClosedTabs
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.browser.browsingmode.BrowsingMode
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

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

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
        assertFalse(RecentlyClosedTabs.menuOpenInNormalTab.testHasValue())

        controller.handleOpen(tabs.toSet(), BrowsingMode.Normal)

        assertEquals(2, tabUrls.size)
        assertEquals(tabs[0].url, tabUrls[0])
        assertEquals(tabs[1].url, tabUrls[1])
        assertEquals(BrowsingMode.Normal, actualBrowsingModes[0])
        assertEquals(BrowsingMode.Normal, actualBrowsingModes[1])
        assertTrue(RecentlyClosedTabs.menuOpenInNormalTab.testHasValue())
        assertNull(RecentlyClosedTabs.menuOpenInNormalTab.testGetValue().last().extra)

        tabUrls.clear()
        actualBrowsingModes.clear()

        controller.handleOpen(tabs.toSet(), BrowsingMode.Private)

        assertEquals(2, tabUrls.size)
        assertEquals(tabs[0].url, tabUrls[0])
        assertEquals(tabs[1].url, tabUrls[1])
        assertEquals(BrowsingMode.Private, actualBrowsingModes[0])
        assertEquals(BrowsingMode.Private, actualBrowsingModes[1])
        assertTrue(RecentlyClosedTabs.menuOpenInPrivateTab.testHasValue())
        assertEquals(1, RecentlyClosedTabs.menuOpenInPrivateTab.testGetValue().size)
        assertNull(RecentlyClosedTabs.menuOpenInPrivateTab.testGetValue().single().extra)
    }

    @Test
    fun `handle selecting first tab`() {
        val selectedTab = createFakeTab()
        every { recentlyClosedStore.state.selectedTabs } returns emptySet()
        assertFalse(RecentlyClosedTabs.enterMultiselect.testHasValue())

        createController().handleSelect(selectedTab)

        verify { recentlyClosedStore.dispatch(RecentlyClosedFragmentAction.Select(selectedTab)) }
        assertTrue(RecentlyClosedTabs.enterMultiselect.testHasValue())
        assertEquals(1, RecentlyClosedTabs.enterMultiselect.testGetValue().size)
        assertNull(RecentlyClosedTabs.enterMultiselect.testGetValue().single().extra)
    }

    @Test
    fun `handle selecting a successive tab`() {
        val selectedTab = createFakeTab()
        every { recentlyClosedStore.state.selectedTabs } returns setOf(mockk())

        createController().handleSelect(selectedTab)

        verify { recentlyClosedStore.dispatch(RecentlyClosedFragmentAction.Select(selectedTab)) }
        assertFalse(RecentlyClosedTabs.enterMultiselect.testHasValue())
    }

    @Test
    fun `handle deselect last tab`() {
        val deselectedTab = createFakeTab()
        every { recentlyClosedStore.state.selectedTabs } returns setOf(deselectedTab)
        assertFalse(RecentlyClosedTabs.exitMultiselect.testHasValue())

        createController().handleDeselect(deselectedTab)

        verify { recentlyClosedStore.dispatch(RecentlyClosedFragmentAction.Deselect(deselectedTab)) }
        assertTrue(RecentlyClosedTabs.exitMultiselect.testHasValue())
        assertEquals(1, RecentlyClosedTabs.exitMultiselect.testGetValue().size)
        assertNull(RecentlyClosedTabs.exitMultiselect.testGetValue().single().extra)
    }

    @Test
    fun `handle deselect a tab from others still selected`() {
        val deselectedTab = createFakeTab()
        every { recentlyClosedStore.state.selectedTabs } returns setOf(deselectedTab, mockk())

        createController().handleDeselect(deselectedTab)

        verify { recentlyClosedStore.dispatch(RecentlyClosedFragmentAction.Deselect(deselectedTab)) }
        assertFalse(RecentlyClosedTabs.exitMultiselect.testHasValue())
    }

    @Test
    fun handleDelete() {
        val item: TabState = mockk(relaxed = true)
        assertFalse(RecentlyClosedTabs.deleteTab.testHasValue())

        createController().handleDelete(item)

        verify {
            browserStore.dispatch(RecentlyClosedAction.RemoveClosedTabAction(item))
        }
        assertTrue(RecentlyClosedTabs.deleteTab.testHasValue())
        assertEquals(1, RecentlyClosedTabs.deleteTab.testGetValue().size)
        assertNull(RecentlyClosedTabs.deleteTab.testGetValue().single().extra)
    }

    @Test
    fun `delete multiple tabs`() {
        val tabs = createFakeTabList(2)
        assertFalse(RecentlyClosedTabs.menuDelete.testHasValue())

        createController().handleDelete(tabs.toSet())

        verify {
            browserStore.dispatch(RecentlyClosedAction.RemoveClosedTabAction(tabs[0]))
            browserStore.dispatch(RecentlyClosedAction.RemoveClosedTabAction(tabs[1]))
        }
        assertTrue(RecentlyClosedTabs.menuDelete.testHasValue())
        assertNull(RecentlyClosedTabs.menuDelete.testGetValue().last().extra)
    }

    @Test
    fun handleNavigateToHistory() {
        assertFalse(RecentlyClosedTabs.showFullHistory.testHasValue())

        createController().handleNavigateToHistory()

        verify {
            navController.navigate(
                directionsEq(
                    RecentlyClosedFragmentDirections.actionGlobalHistoryFragment()
                ),
                optionsEq(NavOptions.Builder().setPopUpTo(R.id.historyFragment, true).build())
            )
        }
        assertTrue(RecentlyClosedTabs.showFullHistory.testHasValue())
        assertEquals(1, RecentlyClosedTabs.showFullHistory.testGetValue().size)
        assertNull(RecentlyClosedTabs.showFullHistory.testGetValue().single().extra)
    }

    @Test
    fun `share multiple tabs`() {
        val tabs = createFakeTabList(2)
        assertFalse(RecentlyClosedTabs.menuShare.testHasValue())

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
        assertTrue(RecentlyClosedTabs.menuShare.testHasValue())
        assertEquals(1, RecentlyClosedTabs.menuShare.testGetValue().size)
        assertNull(RecentlyClosedTabs.menuShare.testGetValue().single().extra)
    }

    @Test
    fun handleRestore() = runTest {
        val item: TabState = mockk(relaxed = true)
        assertFalse(RecentlyClosedTabs.openTab.testHasValue())

        createController(scope = this).handleRestore(item)
        runCurrent()

        coVerify { tabsUseCases.restore.invoke(eq(item), any(), true) }
        assertTrue(RecentlyClosedTabs.openTab.testHasValue())
        assertEquals(1, RecentlyClosedTabs.openTab.testGetValue().size)
        assertNull(RecentlyClosedTabs.openTab.testGetValue().single().extra)
    }

    @Test
    fun `exist multi-select mode when back is pressed`() {
        every { recentlyClosedStore.state.selectedTabs } returns createFakeTabList(3).toSet()
        assertFalse(RecentlyClosedTabs.exitMultiselect.testHasValue())

        createController().handleBackPressed()

        verify { recentlyClosedStore.dispatch(RecentlyClosedFragmentAction.DeselectAll) }
        assertTrue(RecentlyClosedTabs.exitMultiselect.testHasValue())
        assertEquals(1, RecentlyClosedTabs.exitMultiselect.testGetValue().size)
        assertNull(RecentlyClosedTabs.exitMultiselect.testGetValue().single().extra)
    }

    @Test
    fun `report closing the fragment when back is pressed`() {
        every { recentlyClosedStore.state.selectedTabs } returns emptySet()
        assertFalse(RecentlyClosedTabs.closed.testHasValue())

        createController().handleBackPressed()

        verify(exactly = 0) { recentlyClosedStore.dispatch(RecentlyClosedFragmentAction.DeselectAll) }
        assertTrue(RecentlyClosedTabs.closed.testHasValue())
        assertEquals(1, RecentlyClosedTabs.closed.testGetValue().size)
        assertNull(RecentlyClosedTabs.closed.testGetValue().single().extra)
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
