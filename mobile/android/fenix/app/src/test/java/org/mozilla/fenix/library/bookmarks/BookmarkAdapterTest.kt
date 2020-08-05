/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verifyOrder
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
internal class BookmarkAdapterTest {

    private lateinit var bookmarkAdapter: BookmarkAdapter

    private val item = BookmarkNode(
        BookmarkNodeType.ITEM,
        "456",
        "123",
        0,
        "Mozilla",
        "http://mozilla.org",
        null
    )

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
                item,
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
        bookmarkAdapter.updateData(tree, BookmarkFragmentState.Mode.Normal())
        bookmarkAdapter.updateData(null, BookmarkFragmentState.Mode.Normal())
        verifyOrder {
            bookmarkAdapter.updateData(tree, BookmarkFragmentState.Mode.Normal())
            bookmarkAdapter.notifyItemRangeInserted(0, 3)
            bookmarkAdapter.updateData(null, BookmarkFragmentState.Mode.Normal())
            bookmarkAdapter.notifyItemRangeRemoved(0, 3)
        }
    }

    @Test
    fun `items are the same if they have the same guids`() {
        assertTrue(createSingleItemDiffUtil(item, item).areItemsTheSame(0, 0))
        assertTrue(
            createSingleItemDiffUtil(
                item,
                item.copy(title = "Wikipedia.org", url = "https://www.wikipedia.org")
            ).areItemsTheSame(0, 0)
        )
        assertFalse(
            createSingleItemDiffUtil(
                item,
                item.copy(guid = "111")
            ).areItemsTheSame(0, 0)
        )
    }

    @Test
    fun `equal items have same contents unless their selected state changes`() {
        assertTrue(createSingleItemDiffUtil(item, item).areContentsTheSame(0, 0))
        assertFalse(
            createSingleItemDiffUtil(item, item.copy(position = 1)).areContentsTheSame(0, 0)
        )
        assertFalse(
            createSingleItemDiffUtil(
                item,
                item,
                oldMode = BookmarkFragmentState.Mode.Selecting(setOf(item))
            ).areContentsTheSame(0, 0)
        )
        assertFalse(
            createSingleItemDiffUtil(
                item,
                item,
                newMode = BookmarkFragmentState.Mode.Selecting(setOf(item))
            ).areContentsTheSame(0, 0)
        )
    }

    private fun createSingleItemDiffUtil(
        oldItem: BookmarkNode,
        newItem: BookmarkNode,
        oldMode: BookmarkFragmentState.Mode = BookmarkFragmentState.Mode.Normal(),
        newMode: BookmarkFragmentState.Mode = BookmarkFragmentState.Mode.Normal()
    ): BookmarkAdapter.BookmarkDiffUtil {
        return BookmarkAdapter.BookmarkDiffUtil(listOf(oldItem), listOf(newItem), oldMode, newMode)
    }
}
