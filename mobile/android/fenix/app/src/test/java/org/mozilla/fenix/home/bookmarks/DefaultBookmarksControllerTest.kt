/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.bookmarks

import androidx.navigation.NavController
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags.Companion.ALLOW_JAVASCRIPT_URL
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import mozilla.telemetry.glean.testing.GleanTestRule
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.GleanMetrics.HomeBookmarks
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.R
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.home.HomeFragmentDirections
import org.mozilla.fenix.home.bookmarks.controller.DefaultBookmarksController

@RunWith(FenixRobolectricTestRunner::class)
class DefaultBookmarksControllerTest {

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    private val activity: HomeActivity = mockk(relaxed = true)
    private val navController: NavController = mockk(relaxUnitFun = true)
    private val selectTabUseCase: TabsUseCases = mockk(relaxed = true)
    private val browserStore: BrowserStore = mockk(relaxed = true)

    private lateinit var controller: DefaultBookmarksController

    @Before
    fun setup() {
        every { activity.openToBrowserAndLoad(any(), any(), any()) } just Runs
        every { browserStore.state.tabs }.returns(emptyList())

        controller = spyk(
            DefaultBookmarksController(
                activity = activity,
                navController = navController,
                appStore = mockk(),
                browserStore = browserStore,
                selectTabUseCase = selectTabUseCase.selectTab,
            ),
        )
    }

    @Test
    fun `GIVEN no tabs WHEN a bookmark is clicked THEN the selected bookmark is opened in a new tab`() {
        assertNull(HomeBookmarks.bookmarkClicked.testGetValue())

        val bookmark = Bookmark(title = null, url = "https://www.example.com")
        controller.handleBookmarkClicked(bookmark)

        verify {
            activity.openToBrowserAndLoad(
                searchTermOrURL = bookmark.url!!,
                newTab = true,
                flags = EngineSession.LoadUrlFlags.select(ALLOW_JAVASCRIPT_URL),
                from = BrowserDirection.FromHome,
            )
        }
        assertNotNull(HomeBookmarks.bookmarkClicked.testGetValue())
    }

    @Test
    fun `GIVEN no matching tabs WHEN a bookmark is clicked THEN the selected bookmark is opened in a new tab`() {
        assertNull(HomeBookmarks.bookmarkClicked.testGetValue())

        val testTab = createTab("https://www.not_example.com")
        every { browserStore.state.tabs }.returns(listOf(testTab))

        val bookmark = Bookmark(title = null, url = "https://www.example.com")
        controller.handleBookmarkClicked(bookmark)

        verify {
            activity.openToBrowserAndLoad(
                searchTermOrURL = bookmark.url!!,
                newTab = true,
                flags = EngineSession.LoadUrlFlags.select(ALLOW_JAVASCRIPT_URL),
                from = BrowserDirection.FromHome,
            )
        }
        assertNotNull(HomeBookmarks.bookmarkClicked.testGetValue())
    }

    @Test
    fun `GIVEN matching tab WHEN a bookmark is clicked THEN the existing tab is opened`() {
        assertNull(HomeBookmarks.bookmarkClicked.testGetValue())

        val testUrl = "https://www.example.com"
        val testTab = createTab(testUrl)
        every { browserStore.state.tabs }.returns(listOf(testTab))

        val bookmark = Bookmark(title = null, url = testUrl)
        controller.handleBookmarkClicked(bookmark)

        verify {
            selectTabUseCase.invoke(testTab.id)
            navController.navigate(R.id.browserFragment)
        }
        assertNotNull(HomeBookmarks.bookmarkClicked.testGetValue())
    }

    @Test
    fun `WHEN show all bookmarks is clicked THEN the bookmarks root is opened`() = runTestOnMain {
        assertNull(HomeBookmarks.showAllBookmarks.testGetValue())

        controller.handleShowAllBookmarksClicked()

        val directions = HomeFragmentDirections.actionGlobalBookmarkFragment(BookmarkRoot.Mobile.id)
        verify {
            navController.navigate(directions)
        }
        assertNotNull(HomeBookmarks.showAllBookmarks.testGetValue())
    }

    @Test
    fun `WHEN show all bppkmarks is clicked from behind search dialog THEN open bookmarks root`() {
        assertNull(HomeBookmarks.showAllBookmarks.testGetValue())

        controller.handleShowAllBookmarksClicked()

        val directions = HomeFragmentDirections.actionGlobalBookmarkFragment(BookmarkRoot.Mobile.id)

        verify {
            navController.navigate(directions)
        }
        assertNotNull(HomeBookmarks.showAllBookmarks.testGetValue())
    }
}
