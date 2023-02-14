/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verifyOrder
import mozilla.components.concept.storage.BookmarkNode
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.library.bookmarks.viewholders.BookmarkNodeViewHolder

@RunWith(FenixRobolectricTestRunner::class)
internal class BookmarkAdapterTest {

    private lateinit var bookmarkAdapter: BookmarkAdapter

    @Before
    fun setup() {
        bookmarkAdapter = spyk(
            BookmarkAdapter(mockk(relaxed = true), mockk()),
        )
    }

    @Test
    fun `update adapter from tree of bookmark nodes, null tree returns empty list`() {
        val tree = testFolder(
            "123",
            "root",
            listOf(
                testBookmarkItem("someFolder", "http://mozilla.org"),
                testSeparator("123"),
                testBookmarkItem("123", "https://www.mozilla.org/en-US/firefox/"),
            ),
        )
        bookmarkAdapter.updateData(tree, BookmarkFragmentState.Mode.Normal())
        bookmarkAdapter.updateData(null, BookmarkFragmentState.Mode.Normal())
        verifyOrder {
            bookmarkAdapter.updateData(tree, BookmarkFragmentState.Mode.Normal())
            bookmarkAdapter.notifyItemRangeInserted(0, 2)
            bookmarkAdapter.updateData(null, BookmarkFragmentState.Mode.Normal())
            bookmarkAdapter.notifyItemRangeRemoved(0, 2)
        }
    }

    @Test
    fun `update adapter from tree of bookmark nodes, separators are excluded`() {
        val sep1 = testSeparator("123")
        val sep2 = testSeparator("123")
        val item1 = testBookmarkItem("123", "http://mozilla.org")
        val item2 = testBookmarkItem("123", "https://www.mozilla.org/en-US/firefox/")
        val folder = testFolder("123", "root", title = "Mobile", children = listOf(item1, sep1, item2, sep2))
        bookmarkAdapter.updateData(folder, BookmarkFragmentState.Mode.Normal())
        verifyOrder {
            bookmarkAdapter.updateData(folder, BookmarkFragmentState.Mode.Normal())
            bookmarkAdapter.notifyItemRangeInserted(0, 2)
        }

        assertEquals(2, bookmarkAdapter.itemCount)
        assertEquals(listOf(item1, item2), bookmarkAdapter.tree)
    }

    @Test
    fun `update adapter from tree of bookmark nodes, folders are moved to the top`() {
        val sep1 = testSeparator("123")
        val item1 = testBookmarkItem("123", "http://mozilla.org")
        val item2 = testBookmarkItem("123", "https://www.mozilla.org/en-US/firefox/")
        val item3 = testBookmarkItem("123", "https://www.mozilla.org/en-US/firefox/2")
        val item4 = testBookmarkItem("125", "https://www.mozilla.org/en-US/firefox/3")
        val folder2 = testFolder("124", "123", title = "Mobile 2", children = emptyList())
        val folder3 = testFolder("125", "123", title = "Mobile 3", children = listOf(item4))
        val folder4 = testFolder("126", "123", title = "Mobile 3", children = emptyList())
        val folder = testFolder(
            "123",
            "root",
            title = "Mobile",
            children = listOf(
                folder4,
                item1,
                sep1,
                item2,
                folder2,
                folder3,
                item3,
            ),
        )
        bookmarkAdapter.updateData(folder, BookmarkFragmentState.Mode.Normal())
        verifyOrder {
            bookmarkAdapter.updateData(folder, BookmarkFragmentState.Mode.Normal())
            bookmarkAdapter.notifyItemRangeInserted(0, 6)
        }

        assertEquals(6, bookmarkAdapter.itemCount)
        assertEquals(listOf(folder4, folder2, folder3, item1, item2, item3), bookmarkAdapter.tree)
    }

    @Test
    fun `get item view type for different types of nodes`() {
        val sep1 = testSeparator("123")
        val item1 = testBookmarkItem("123", "https://www.mozilla.org/en-US/firefox/")
        val folder1 = testFolder("124", "123", title = "Mobile 2", children = emptyList())
        bookmarkAdapter.updateData(
            testFolder("123", "root", listOf(sep1, item1, folder1)),
            BookmarkFragmentState.Mode.Normal(),
        )

        assertEquals(2, bookmarkAdapter.itemCount)
        // item1
        assertEquals(BookmarkNodeViewHolder.LAYOUT_ID, bookmarkAdapter.getItemViewType(0))
        // folder1
        assertEquals(BookmarkNodeViewHolder.LAYOUT_ID, bookmarkAdapter.getItemViewType(1))
        // sep is dropped during update
    }

    @Test
    fun `items are the same if they have the same guids`() {
        val item = testBookmarkItem("someFolder", "http://mozilla.org")
        assertTrue(createSingleItemDiffUtil(item, item).areItemsTheSame(0, 0))
        assertTrue(
            createSingleItemDiffUtil(
                item,
                item.copy(title = "Wikipedia.org", url = "https://www.wikipedia.org"),
            ).areItemsTheSame(0, 0),
        )
        assertFalse(
            createSingleItemDiffUtil(
                item,
                item.copy(guid = "111"),
            ).areItemsTheSame(0, 0),
        )
    }

    @Test
    fun `equal items have same contents unless their selected state changes`() {
        val item = testBookmarkItem("someFolder", "http://mozilla.org")
        assertTrue(createSingleItemDiffUtil(item, item).areContentsTheSame(0, 0))
        assertFalse(
            createSingleItemDiffUtil(item, item.copy(position = 1u)).areContentsTheSame(0, 0),
        )
        assertFalse(
            createSingleItemDiffUtil(
                item,
                item,
                oldMode = BookmarkFragmentState.Mode.Selecting(setOf(item)),
            ).areContentsTheSame(0, 0),
        )
        assertFalse(
            createSingleItemDiffUtil(
                item,
                item,
                newMode = BookmarkFragmentState.Mode.Selecting(setOf(item)),
            ).areContentsTheSame(0, 0),
        )
    }

    private fun createSingleItemDiffUtil(
        oldItem: BookmarkNode,
        newItem: BookmarkNode,
        oldMode: BookmarkFragmentState.Mode = BookmarkFragmentState.Mode.Normal(),
        newMode: BookmarkFragmentState.Mode = BookmarkFragmentState.Mode.Normal(),
    ): BookmarkAdapter.BookmarkDiffUtil {
        return BookmarkAdapter.BookmarkDiffUtil(listOf(oldItem), listOf(newItem), oldMode, newMode)
    }
}
