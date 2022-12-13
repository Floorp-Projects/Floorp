/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recentbookmarks

import androidx.navigation.NavController
import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags.Companion.ALLOW_JAVASCRIPT_URL
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
import org.mozilla.fenix.GleanMetrics.RecentBookmarks
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.home.HomeFragmentDirections
import org.mozilla.fenix.home.recentbookmarks.controller.DefaultRecentBookmarksController

@OptIn(ExperimentalCoroutinesApi::class)
@RunWith(FenixRobolectricTestRunner::class)
class DefaultRecentBookmarksControllerTest {

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    private val activity: HomeActivity = mockk(relaxed = true)
    private val navController: NavController = mockk(relaxUnitFun = true)

    private lateinit var controller: DefaultRecentBookmarksController

    @Before
    fun setup() {
        every { activity.openToBrowserAndLoad(any(), any(), any()) } just Runs

        controller = spyk(
            DefaultRecentBookmarksController(
                activity = activity,
                navController = navController,
                appStore = mockk(),
            ),
        )
    }

    @Test
    fun `WHEN a recently saved bookmark is clicked THEN the selected bookmark is opened`() {
        assertNull(RecentBookmarks.bookmarkClicked.testGetValue())

        val bookmark = RecentBookmark(
            title = null,
            url = "https://www.example.com",
        )

        controller.handleBookmarkClicked(bookmark)

        verify {
            activity.openToBrowserAndLoad(
                searchTermOrURL = bookmark.url!!,
                newTab = true,
                flags = EngineSession.LoadUrlFlags.select(ALLOW_JAVASCRIPT_URL),
                from = BrowserDirection.FromHome,
            )
        }
        assertNotNull(RecentBookmarks.bookmarkClicked.testGetValue())
    }

    @Test
    fun `WHEN show all recently saved bookmark is clicked THEN the bookmarks root is opened`() = runTestOnMain {
        assertNull(RecentBookmarks.showAllBookmarks.testGetValue())

        controller.handleShowAllBookmarksClicked()

        val directions = HomeFragmentDirections.actionGlobalBookmarkFragment(BookmarkRoot.Mobile.id)
        verify {
            navController.navigate(directions)
        }
        assertNotNull(RecentBookmarks.showAllBookmarks.testGetValue())
    }

    @Test
    fun `WHEN show all is clicked from behind search dialog THEN open bookmarks root`() {
        assertNull(RecentBookmarks.showAllBookmarks.testGetValue())

        controller.handleShowAllBookmarksClicked()

        val directions = HomeFragmentDirections.actionGlobalBookmarkFragment(BookmarkRoot.Mobile.id)

        verify {
            navController.navigate(directions)
        }
        assertNotNull(RecentBookmarks.showAllBookmarks.testGetValue())
    }
}
