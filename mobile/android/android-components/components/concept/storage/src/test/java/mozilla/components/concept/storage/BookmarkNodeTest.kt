/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.storage

import org.junit.Assert.assertEquals
import org.junit.Test

class BookmarkNodeTest {

    private val bookmarkChild1 = testBookmarkItem(
        url = "http://www.mockurl.com/1",
        title = "Child 1",
    )
    private val bookmarkChild2 = testBookmarkItem(
        url = "http://www.mockurl.com/2",
        title = "Child 2",
    )
    private val bookmarkChild3 = testBookmarkItem(
        url = "http://www.mockurl.com/3",
        title = "Child 3",
    )
    private val allChildren = listOf(bookmarkChild1, bookmarkChild2)

    @Test
    fun `GIVEN a bookmark node with children WHEN subtracting a sub set of children THEN the children subset is removed and rest remains`() {
        val bookmarkNode = testFolder("parent1", "root", allChildren)
        val subsetToSubtract = setOf(bookmarkChild1)
        val expectedRemainingSubset = listOf(bookmarkChild2)
        val bookmarkNodeSubsetRemoved = bookmarkNode.minus(subsetToSubtract)
        assertEquals(expectedRemainingSubset, bookmarkNodeSubsetRemoved.children)
    }

    @Test
    fun `GIVEN a bookmark node with children WHEN subtracting a set of all children THEN all children are removed and empty list remains`() {
        val bookmarkNode = testFolder("parent1", "root", allChildren)
        val setOfAllChildren = setOf(bookmarkChild1, bookmarkChild2)
        val bookmarkNodeAllChildrenRemoved = bookmarkNode.minus(setOfAllChildren)
        assertEquals(emptyList<BookmarkNode>(), bookmarkNodeAllChildrenRemoved.children)
    }

    @Test
    fun `GIVEN a bookmark node with children WHEN subtracting a set of non-children THEN no children are removed`() {
        val setOfNonChildren = setOf(bookmarkChild3)
        val bookmarkNode = testFolder("parent1", "root", allChildren)
        val bookmarkNodeNonChildrenRemoved = bookmarkNode.minus(setOfNonChildren)
        assertEquals(allChildren, bookmarkNodeNonChildrenRemoved.children)
    }

    @Test
    fun `GIVEN a bookmark node with children WHEN subtracting an empty set THEN no children are removed`() {
        val bookmarkNode = testFolder("parent1", "root", allChildren)
        val bookmarkNodeEmptySetRemoved = bookmarkNode.minus(emptySet())
        assertEquals(allChildren, bookmarkNodeEmptySetRemoved.children)
    }

    @Test
    fun `GIVEN a bookmark node with an empty list as children WHEN subtracting a set of non-children from an empty parent THEN an empty list remains`() {
        val parentWithEmptyList = testFolder("parent1", "root", emptyList())
        val setOfAllChildren = setOf(bookmarkChild1, bookmarkChild2)
        val parentWithEmptyListNonChildRemoved = parentWithEmptyList.minus(setOfAllChildren)
        assertEquals(emptyList<BookmarkNode>(), parentWithEmptyListNonChildRemoved.children)
    }

    @Test
    fun `GIVEN a bookmark node with null as children WHEN subtracting a set of non-children from a parent with null children THEN null remains`() {
        val parentWithNullList = testFolder("parent1", "root", null)
        val parentWithNullListNonChildRemoved = parentWithNullList.minus(allChildren.toSet())
        assertEquals(null, parentWithNullListNonChildRemoved.children)
    }

    @Test
    fun `GIVEN a bookmark node with children WHEN subtracting a sub-set of children THEN the rest of the parents object should remain the same`() {
        val bookmarkNode = testFolder("parent1", "root", allChildren)
        val subsetToSubtract = setOf(bookmarkChild1)
        val expectedRemainingSubset = listOf(bookmarkChild2)
        val resultBookmarkNode = bookmarkNode.minus(subsetToSubtract)

        // We're pinning children to the same value so we can compare the rest.
        val restOfResult = resultBookmarkNode.copy(children = expectedRemainingSubset)
        val restOfOriginal = bookmarkNode.copy(children = expectedRemainingSubset)
        assertEquals(restOfResult, restOfOriginal)
    }

    @Test
    fun `GIVEN a bookmark node with two child items THEN count should be two`() {
        val bookmarksFolder = testFolder(
            guid = "folder1",
            children = allChildren,
        )
        val expected = 2
        val actual = bookmarksFolder.count()
        assertEquals(expected, actual)
    }

    @Test
    fun `GIVEN a bookmark node with no items THEN count should be zero`() {
        val bookmarksFolder = testFolder(
            guid = "folder1",
            children = emptyList(),
        )
        val expected = 0
        val actual = bookmarksFolder.count()
        assertEquals(expected, actual)
    }

    @Test
    fun `GIVEN a bookmark node with null children THEN count should be zero`() {
        val bookmarksFolder = testFolder(
            guid = "folder1",
            children = null,
        )
        val expected = 0
        val actual = bookmarksFolder.count()
        assertEquals(expected, actual)
    }

    @Test
    fun `GIVEN a bookmark node with two children folders with items of their own THEN count should be the number of items`() {
        val bookmarks = testFolder(
            guid = "folder1",
            children = listOf(
                testFolder(
                    guid = "folder1",
                    children = listOf(bookmarkChild1, bookmarkChild2),
                ),
                bookmarkChild1,
                bookmarkChild2,
                bookmarkChild3,
                testFolder(
                    "folder2",
                    children = listOf(bookmarkChild3),
                ),
            ),
        )

        val expected = 6
        val actual = bookmarks.count()
        assertEquals(expected, actual)
    }

    @Test
    fun `GIVEN a bookmark node with two level deep nested folders with items of their own THEN count should be the number of items`() {
        val folder = testFolder(
            guid = "folder1",
            children = listOf(bookmarkChild1, bookmarkChild2),
        )
        val bookmarks = testFolder(
            guid = "folder1",
            children = listOf(
                folder,
                bookmarkChild1,
                bookmarkChild2,
                bookmarkChild3,
                testFolder(
                    "folder2",
                    children = listOf(bookmarkChild3, folder),
                ),
            ),
        )

        val expected = 8
        val actual = bookmarks.count()
        assertEquals(expected, actual)
    }

    @Test
    fun `GIVEN a bookmark node with two child items and a separator THEN count should be two`() {
        val separator = BookmarkNode(
            type = BookmarkNodeType.SEPARATOR,
            guid = "sep",
            parentGuid = "folder1",
            position = null,
            title = null,
            url = null,
            dateAdded = 0,
            children = null,
        )

        val bookmarksFolder = testFolder(
            guid = "folder1",
            children = listOf(bookmarkChild1, separator, bookmarkChild2),
        )
        val expected = 2
        val actual = bookmarksFolder.count()
        assertEquals(expected, actual)
    }

    private fun testBookmarkItem(
        parentGuid: String = "someFolder",
        url: String,
        title: String = "Item for $url",
        guid: String = "guid#${Math.random() * 1000}",
        position: UInt = 0u,
    ) = BookmarkNode(
        type = BookmarkNodeType.ITEM,
        dateAdded = 0,
        children = null,
        guid = guid,
        parentGuid = parentGuid,
        position = position,
        title = title,
        url = url,
    )

    private fun testFolder(
        guid: String,
        parentGuid: String? = null,
        children: List<BookmarkNode>?,
        title: String = "Folder: $guid",
        position: UInt = 0u,
    ) = BookmarkNode(
        type = BookmarkNodeType.FOLDER,
        url = null,
        dateAdded = 0,
        guid = guid,
        parentGuid = parentGuid,
        position = position,
        title = title,
        children = children,
    )
}
