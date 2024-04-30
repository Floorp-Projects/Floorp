/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.bookmarks

import io.mockk.coEvery
import io.mockk.coVerify
import io.mockk.mockk
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.advanceUntilIdle
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.middleware.CaptureActionsMiddleware
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.components.bookmarks.BookmarksUseCase

@OptIn(ExperimentalCoroutinesApi::class)
class BookmarksFeatureTest {

    private val middleware = CaptureActionsMiddleware<AppState, AppAction>()
    private val appStore = AppStore(middlewares = listOf(middleware))
    private val bookmarksUseCases: BookmarksUseCase = mockk(relaxed = true)
    private val bookmark = Bookmark(
        title = null,
        url = "https://www.example.com",
        previewImageUrl = null,
    )

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val testDispatcher = coroutinesTestRule.testDispatcher
    private val scope = coroutinesTestRule.scope

    @Before
    fun setup() {
        coEvery { bookmarksUseCases.retrieveRecentBookmarks() }.coAnswers { listOf(bookmark) }
    }

    @Test
    fun `GIVEN no bookmarks WHEN feature starts THEN fetch bookmarks and notify store`() =
        runTestOnMain {
            val feature = BookmarksFeature(
                appStore,
                bookmarksUseCases,
                scope,
                testDispatcher,
            )

            assertEquals(emptyList<BookmarkNode>(), appStore.state.bookmarks)

            feature.start()

            advanceUntilIdle()
            appStore.waitUntilIdle()

            coVerify {
                bookmarksUseCases.retrieveRecentBookmarks()
            }

            middleware.assertLastAction(AppAction.BookmarksChange::class) {
                assertEquals(listOf(bookmark), it.bookmarks)
            }
        }
}
