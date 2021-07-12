/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recentbookmarks

import io.mockk.coEvery
import io.mockk.coVerify
import io.mockk.mockk
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.TestCoroutineDispatcher
import kotlinx.coroutines.test.runBlockingTest
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.middleware.CaptureActionsMiddleware
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.components.bookmarks.BookmarksUseCase
import org.mozilla.fenix.home.HomeFragmentAction
import org.mozilla.fenix.home.HomeFragmentState
import org.mozilla.fenix.home.HomeFragmentStore

@OptIn(ExperimentalCoroutinesApi::class)
class RecentBookmarksFeatureTest {

    private val middleware = CaptureActionsMiddleware<HomeFragmentState, HomeFragmentAction>()
    private val homeStore = HomeFragmentStore(middlewares = listOf(middleware))
    private val bookmarksUseCases: BookmarksUseCase = mockk(relaxed = true)
    private val testDispatcher = TestCoroutineDispatcher()
    private val bookmark = BookmarkNode(
        type = BookmarkNodeType.ITEM,
        guid = "guid#${Math.random() * 1000}",
        parentGuid = null,
        position = null,
        title = null,
        url = "https://www.example.com",
        children = null
    )

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule(testDispatcher)

    @Before
    fun setup() {
        coEvery { bookmarksUseCases.retrieveRecentBookmarks() }.coAnswers { listOf(bookmark) }
    }

    @After
    fun cleanUp() {
        testDispatcher.cleanupTestCoroutines()
    }

    @Test
    fun `GIVEN no recent bookmarks WHEN feature starts THEN fetch bookmarks and notify store`() =
        testDispatcher.runBlockingTest {
            val feature = RecentBookmarksFeature(
                homeStore,
                bookmarksUseCases,
                CoroutineScope(testDispatcher),
                testDispatcher
            )

            assertEquals(emptyList<BookmarkNode>(), homeStore.state.recentBookmarks)

            feature.start()

            testDispatcher.advanceUntilIdle()
            homeStore.waitUntilIdle()

            coVerify {
                bookmarksUseCases.retrieveRecentBookmarks()
            }

            middleware.assertLastAction(HomeFragmentAction.RecentBookmarksChange::class) {
                assertEquals(listOf(bookmark), it.recentBookmarks)
            }
        }
}
