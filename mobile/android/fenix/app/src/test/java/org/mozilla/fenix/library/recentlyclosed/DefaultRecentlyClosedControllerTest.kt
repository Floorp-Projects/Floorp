/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.recentlyclosed

import android.content.ClipData
import android.content.ClipboardManager
import android.content.res.Resources
import androidx.navigation.NavController
import androidx.navigation.NavOptions
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.slot
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
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
import org.mozilla.fenix.components.FenixSnackbar
import org.mozilla.fenix.ext.directionsEq
import org.mozilla.fenix.ext.optionsEq
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

// Robolectric needed for `onShareItem()`
@ExperimentalCoroutinesApi
@RunWith(FenixRobolectricTestRunner::class)
class DefaultRecentlyClosedControllerTest {
    private val dispatcher = TestCoroutineDispatcher()
    private val navController: NavController = mockk(relaxed = true)
    private val resources: Resources = mockk(relaxed = true)
    private val snackbar: FenixSnackbar = mockk(relaxed = true)
    private val clipboardManager: ClipboardManager = mockk(relaxed = true)
    private val openToBrowser: (RecoverableTab, BrowsingMode?) -> Unit = mockk(relaxed = true)
    private val activity: HomeActivity = mockk(relaxed = true)
    private val browserStore: BrowserStore = mockk(relaxed = true)
    private val recentlyClosedStore: RecentlyClosedFragmentStore = mockk(relaxed = true)
    private val tabsUseCases: TabsUseCases = mockk(relaxed = true)
    val mockedTab: RecoverableTab = mockk(relaxed = true)

    private val controller = DefaultRecentlyClosedController(
        navController,
        browserStore,
        recentlyClosedStore,
        tabsUseCases,
        resources,
        snackbar,
        clipboardManager,
        activity,
        openToBrowser
    )

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

        controller.handleOpen(item, BrowsingMode.Private)

        verify {
            openToBrowser(item, BrowsingMode.Private)
        }

        controller.handleOpen(item, BrowsingMode.Normal)

        verify {
            openToBrowser(item, BrowsingMode.Normal)
        }
    }

    @Test
    fun `open multiple tabs`() {
        val tabs = createFakeTabList(2)

        controller.handleOpen(tabs.toSet(), BrowsingMode.Normal)

        verify {
            openToBrowser(tabs[0], BrowsingMode.Normal)
            openToBrowser(tabs[1], BrowsingMode.Normal)
        }

        controller.handleOpen(tabs.toSet(), BrowsingMode.Private)

        verify {
            openToBrowser(tabs[0], BrowsingMode.Private)
            openToBrowser(tabs[1], BrowsingMode.Private)
        }
    }

    @Test
    fun `handle select tab`() {
        val selectedTab = createFakeTab()

        controller.handleSelect(selectedTab)

        verify { recentlyClosedStore.dispatch(RecentlyClosedFragmentAction.Select(selectedTab)) }
    }

    @Test
    fun `handle deselect tab`() {
        val deselectedTab = createFakeTab()

        controller.handleDeselect(deselectedTab)

        verify { recentlyClosedStore.dispatch(RecentlyClosedFragmentAction.Deselect(deselectedTab)) }
    }

    @Test
    fun handleDelete() {
        val item: RecoverableTab = mockk(relaxed = true)

        controller.handleDelete(item)

        verify {
            browserStore.dispatch(RecentlyClosedAction.RemoveClosedTabAction(item))
        }
    }

    @Test
    fun `delete multiple tabs`() {
        val tabs = createFakeTabList(2)

        controller.handleDelete(tabs.toSet())

        verify {
            browserStore.dispatch(RecentlyClosedAction.RemoveClosedTabAction(tabs[0]))
            browserStore.dispatch(RecentlyClosedAction.RemoveClosedTabAction(tabs[1]))
        }
    }

    @Test
    fun handleNavigateToHistory() {
        controller.handleNavigateToHistory()

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
    fun handleCopyUrl() {
        val item = RecoverableTab(id = "tab-id", title = "Mozilla", url = "mozilla.org", lastAccess = 1L)

        val clipdata = slot<ClipData>()

        controller.handleCopyUrl(item)

        verify {
            clipboardManager.setPrimaryClip(capture(clipdata))
            snackbar.show()
        }

        assertEquals(1, clipdata.captured.itemCount)
        assertEquals("mozilla.org", clipdata.captured.description.label)
        assertEquals("mozilla.org", clipdata.captured.getItemAt(0).text)
    }

    @Test
    @Suppress("UNCHECKED_CAST")
    fun handleShare() {
        val item = RecoverableTab(id = "tab-id", title = "Mozilla", url = "mozilla.org", lastAccess = 1L)

        controller.handleShare(item)

        verify {
            navController.navigate(
                directionsEq(
                    RecentlyClosedFragmentDirections.actionGlobalShareFragment(
                        data = arrayOf(ShareData(url = item.url, title = item.title))
                    )
                )
            )
        }
    }

    @Test
    fun `share multiple tabs`() {
        val tabs = createFakeTabList(2)

        controller.handleShare(tabs.toSet())

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
        controller.handleRestore(mockedTab)

        dispatcher.advanceUntilIdle()

        verify { tabsUseCases.restore.invoke(mockedTab, true) }
    }

    @Test
    fun `exist multi-select mode when back is pressed`() {
        every { recentlyClosedStore.state.selectedTabs } returns createFakeTabList(3).toSet()

        controller.handleBackPressed()

        verify { recentlyClosedStore.dispatch(RecentlyClosedFragmentAction.DeselectAll) }
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
