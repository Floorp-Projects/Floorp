/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.recentlyclosed

import androidx.navigation.NavController
import androidx.navigation.NavOptions
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.verify
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

        actualTabs.clear()
        actualBrowsingModes.clear()

        controller.handleOpen(tabs.toSet(), BrowsingMode.Private)

        assertEquals(2, actualTabs.size)
        assertEquals(tabs[0], actualTabs[0])
        assertEquals(tabs[1], actualTabs[1])
        assertEquals(BrowsingMode.Private, actualBrowsingModes[0])
        assertEquals(BrowsingMode.Private, actualBrowsingModes[1])
    }

    @Test
    fun `handle select tab`() {
        val selectedTab = createFakeTab()

        createController().handleSelect(selectedTab)

        verify { recentlyClosedStore.dispatch(RecentlyClosedFragmentAction.Select(selectedTab)) }
    }

    @Test
    fun `handle deselect tab`() {
        val deselectedTab = createFakeTab()

        createController().handleDeselect(deselectedTab)

        verify { recentlyClosedStore.dispatch(RecentlyClosedFragmentAction.Deselect(deselectedTab)) }
    }

    @Test
    fun handleDelete() {
        val item: RecoverableTab = mockk(relaxed = true)

        createController().handleDelete(item)

        verify {
            browserStore.dispatch(RecentlyClosedAction.RemoveClosedTabAction(item))
        }
    }

    @Test
    fun `delete multiple tabs`() {
        val tabs = createFakeTabList(2)

        createController().handleDelete(tabs.toSet())

        verify {
            browserStore.dispatch(RecentlyClosedAction.RemoveClosedTabAction(tabs[0]))
            browserStore.dispatch(RecentlyClosedAction.RemoveClosedTabAction(tabs[1]))
        }
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
    }

    @Test
    fun handleRestore() {
        val item: RecoverableTab = mockk(relaxed = true)

        createController().handleRestore(item)

        dispatcher.advanceUntilIdle()

        verify { tabsUseCases.restore.invoke(item, true) }
    }

    @Test
    fun `exist multi-select mode when back is pressed`() {
        every { recentlyClosedStore.state.selectedTabs } returns createFakeTabList(3).toSet()

        createController().handleBackPressed()

        verify { recentlyClosedStore.dispatch(RecentlyClosedFragmentAction.DeselectAll) }
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
