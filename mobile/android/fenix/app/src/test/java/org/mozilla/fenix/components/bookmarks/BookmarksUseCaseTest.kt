/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.bookmarks

import io.mockk.coEvery
import io.mockk.coVerify
import io.mockk.every
import io.mockk.mockk
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.test.runBlockingTest
import mozilla.appservices.places.BookmarkRoot
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarksStorage
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

@ExperimentalCoroutinesApi
class BookmarksUseCaseTest {

    @Test
    fun `WHEN adding existing bookmark THEN no new item is stored`() = runBlockingTest {
        val storage = mockk<BookmarksStorage>()
        val bookmarkNode = mockk<BookmarkNode>()
        val useCase = BookmarksUseCase(storage)

        every { bookmarkNode.url }.answers { "https://mozilla.org" }
        coEvery { storage.getBookmarksWithUrl(any()) }.coAnswers { listOf(bookmarkNode) }

        val result = useCase.addBookmark("https://mozilla.org", "Mozilla")

        assertFalse(result)
    }

    @Test
    fun `WHEN adding bookmark THEN new item is stored`() = runBlockingTest {
        val storage = mockk<BookmarksStorage>(relaxed = true)
        val useCase = BookmarksUseCase(storage)

        coEvery { storage.getBookmarksWithUrl(any()) }.coAnswers { emptyList() }

        val result = useCase.addBookmark("https://mozilla.org", "Mozilla")

        assertTrue(result)

        coVerify { storage.addItem(BookmarkRoot.Mobile.id, "https://mozilla.org", "Mozilla", null) }
    }
}
