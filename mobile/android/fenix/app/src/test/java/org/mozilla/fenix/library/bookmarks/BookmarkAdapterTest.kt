/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verifyOrder
import kotlinx.coroutines.ObsoleteCoroutinesApi
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.TestApplication
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@ObsoleteCoroutinesApi
@RunWith(RobolectricTestRunner::class)
@Config(application = TestApplication::class)
internal class BookmarkAdapterTest {

    private lateinit var bookmarkAdapter: BookmarkAdapter

    @Before
    fun setup() {
        bookmarkAdapter = spyk(
            BookmarkAdapter(mockk(relaxed = true), mockk())
        )
    }

    @Test
    fun `update adapter from tree of bookmark nodes, null tree returns empty list`() {
        val tree = BookmarkNode(
            BookmarkNodeType.FOLDER, "123", null, 0, "Mobile", null, listOf(
                BookmarkNode(BookmarkNodeType.ITEM, "456", "123", 0, "Mozilla", "http://mozilla.org", null),
                BookmarkNode(BookmarkNodeType.SEPARATOR, "789", "123", 1, null, null, null),
                BookmarkNode(
                    BookmarkNodeType.ITEM,
                    "987",
                    "123",
                    2,
                    "Firefox",
                    "https://www.mozilla.org/en-US/firefox/",
                    null
                )
            )
        )
        bookmarkAdapter.updateData(tree, BookmarkState.Mode.Normal)
        bookmarkAdapter.updateData(null, BookmarkState.Mode.Normal)
        verifyOrder {
            bookmarkAdapter.updateData(tree, BookmarkState.Mode.Normal)
            bookmarkAdapter.notifyItemRangeInserted(0, 3)
            bookmarkAdapter.updateData(null, BookmarkState.Mode.Normal)
            bookmarkAdapter.notifyItemRangeRemoved(0, 3)
        }
    }
}
