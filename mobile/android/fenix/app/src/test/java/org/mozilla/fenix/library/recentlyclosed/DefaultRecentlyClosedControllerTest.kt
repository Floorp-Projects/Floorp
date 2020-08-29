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
import io.mockk.mockkStatic
import io.mockk.slot
import io.mockk.unmockkStatic
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineDispatcher
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.state.action.RecentlyClosedAction
import mozilla.components.browser.state.state.ClosedTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.prompt.ShareData
import mozilla.components.feature.recentlyclosed.ext.restoreTab
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
    private val openToBrowser: (ClosedTab, BrowsingMode?) -> Unit = mockk(relaxed = true)
    private val sessionManager: SessionManager = mockk(relaxed = true)
    private val activity: HomeActivity = mockk(relaxed = true)
    private val store: BrowserStore = mockk(relaxed = true)
    val mockedTab: ClosedTab = mockk(relaxed = true)

    private val controller = DefaultRecentlyClosedController(
        navController,
        store,
        sessionManager,
        resources,
        snackbar,
        clipboardManager,
        activity,
        openToBrowser
    )

    @Before
    fun setUp() {
        mockkStatic("mozilla.components.feature.recentlyclosed.ext.ClosedTabKt")
        every { mockedTab.restoreTab(any(), any(), any()) } just Runs
    }

    @After
    fun tearDown() {
        dispatcher.cleanupTestCoroutines()
        unmockkStatic("mozilla.components.feature.recentlyclosed.ext.ClosedTabKt")
    }

    @Test
    fun handleOpen() {
        val item: ClosedTab = mockk(relaxed = true)

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
    fun handleDeleteOne() {
        val item: ClosedTab = mockk(relaxed = true)

        controller.handleDeleteOne(item)

        verify {
            store.dispatch(RecentlyClosedAction.RemoveClosedTabAction(item))
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
        val item = ClosedTab(id = "tab-id", title = "Mozilla", url = "mozilla.org", createdAt = 1L)

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
        val item = ClosedTab(id = "tab-id", title = "Mozilla", url = "mozilla.org", createdAt = 1L)

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
    fun handleRestore() {
        controller.handleRestore(mockedTab)

        dispatcher.advanceUntilIdle()

        verify {
            mockedTab.restoreTab(
                store,
                sessionManager,
                onTabRestored = any()
            )
        }
    }
}
