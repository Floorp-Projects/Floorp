/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.bookmarks

import io.mockk.coEvery
import io.mockk.coVerify
import io.mockk.every
import io.mockk.mockk
import kotlinx.coroutines.test.runTest
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import mozilla.components.concept.storage.BookmarksStorage
import mozilla.components.concept.storage.HistoryStorage
import mozilla.components.concept.storage.VisitInfo
import mozilla.components.concept.storage.VisitType
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mozilla.fenix.home.recentbookmarks.RecentBookmark
import java.util.concurrent.TimeUnit

class BookmarksUseCaseTest {

    @Test
    fun `WHEN adding existing bookmark THEN no new item is stored`() = runTest {
        val bookmarksStorage = mockk<BookmarksStorage>()
        val historyStorage = mockk<HistoryStorage>()
        val bookmarkNode = mockk<BookmarkNode>()
        val useCase = BookmarksUseCase(bookmarksStorage, historyStorage)

        every { bookmarkNode.url }.answers { "https://mozilla.org" }
        coEvery { bookmarksStorage.getBookmarksWithUrl(any()) }.coAnswers { listOf(bookmarkNode) }

        val result = useCase.addBookmark("https://mozilla.org", "Mozilla")

        assertFalse(result)
    }

    @Test
    fun `WHEN adding bookmark THEN new item is stored`() = runTest {
        val bookmarksStorage = mockk<BookmarksStorage>(relaxed = true)
        val historyStorage = mockk<HistoryStorage>(relaxed = true)
        val bookmarkNode = mockk<BookmarkNode>()
        val useCase = BookmarksUseCase(bookmarksStorage, historyStorage)

        every { bookmarkNode.url }.answers { "https://firefox.com" }
        coEvery { bookmarksStorage.getBookmarksWithUrl(any()) }.coAnswers { listOf(bookmarkNode) }

        val result = useCase.addBookmark("https://mozilla.org", "Mozilla")

        assertTrue(result)

        coVerify { bookmarksStorage.addItem(BookmarkRoot.Mobile.id, "https://mozilla.org", "Mozilla", null) }
    }

    @Test
    fun `WHEN adding bookmark THEN new item is stored in folder`() = runTest {
        val bookmarksStorage = mockk<BookmarksStorage>(relaxed = true)
        val historyStorage = mockk<HistoryStorage>(relaxed = true)
        val bookmarkNode = mockk<BookmarkNode>()
        val useCase = BookmarksUseCase(bookmarksStorage, historyStorage)

        every { bookmarkNode.url }.answers { "https://firefox.com" }
        coEvery { bookmarksStorage.getBookmarksWithUrl(any()) }.coAnswers { listOf(bookmarkNode) }

        val result = useCase.addBookmark("https://mozilla.org", "Mozilla", parentGuid = "parentGuid")

        assertTrue(result)

        coVerify { bookmarksStorage.addItem("parentGuid", "https://mozilla.org", "Mozilla", null) }
    }

    @Test
    fun `WHEN recently saved bookmarks exist THEN retrieve the list from storage`() = runTest {
        val bookmarksStorage = mockk<BookmarksStorage>(relaxed = true)
        val historyStorage = mockk<HistoryStorage>(relaxed = true)
        val useCase = BookmarksUseCase(bookmarksStorage, historyStorage)

        val visitInfo = VisitInfo(
            url = "https://www.firefox.com",
            title = "firefox",
            visitTime = 2,
            visitType = VisitType.LINK,
            previewImageUrl = "http://firefox.com/image1",
            isRemote = false,
        )
        val bookmarkNode = BookmarkNode(
            BookmarkNodeType.ITEM,
            "987",
            "123",
            2u,
            "Firefox",
            "https://www.firefox.com",
            0,
            null,
        )

        coEvery {
            historyStorage.getDetailedVisits(any(), any())
        }.coAnswers { listOf(visitInfo) }

        coEvery {
            bookmarksStorage.getRecentBookmarks(
                any(),
                any(),
                any(),
            )
        }.coAnswers { listOf(bookmarkNode) }

        val result = useCase.retrieveRecentBookmarks(BookmarksUseCase.DEFAULT_BOOKMARKS_TO_RETRIEVE, 22)

        assertEquals(
            listOf(
                RecentBookmark(
                    title = bookmarkNode.title,
                    url = bookmarkNode.url,
                    previewImageUrl = visitInfo.previewImageUrl,
                ),
            ),
            result,
        )

        coVerify {
            bookmarksStorage.getRecentBookmarks(
                BookmarksUseCase.DEFAULT_BOOKMARKS_TO_RETRIEVE,
                22,
                any(),
            )
        }
    }

    @Test
    fun `WHEN there are no recently saved bookmarks THEN retrieve the empty list from storage`() = runTest {
        val bookmarksStorage = mockk<BookmarksStorage>(relaxed = true)
        val historyStorage = mockk<HistoryStorage>(relaxed = true)
        val useCase = BookmarksUseCase(bookmarksStorage, historyStorage)

        coEvery { bookmarksStorage.getRecentBookmarks(any(), any(), any()) }.coAnswers { listOf() }

        val result = useCase.retrieveRecentBookmarks(BookmarksUseCase.DEFAULT_BOOKMARKS_TO_RETRIEVE)

        assertEquals(listOf<BookmarkNode>(), result)

        coVerify {
            bookmarksStorage.getRecentBookmarks(
                BookmarksUseCase.DEFAULT_BOOKMARKS_TO_RETRIEVE,
                TimeUnit.DAYS.toMillis(BookmarksUseCase.DEFAULT_BOOKMARKS_DAYS_AGE_TO_RETRIEVE),
                any(),
            )
        }
    }
}
