/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.TestApplication
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType

@RunWith(RobolectricTestRunner::class)
@Config(application = TestApplication::class)
class BookmarkNodeTest {

    @Test
    fun `GIVEN a bookmark node with children WHEN minusing a sub set of children THEN the children subset is removed and rest remains`() {
        val bookMarkChild1 = newBookmarkNode("Child 1", 1, null)
        val bookMarkChild2 = newBookmarkNode("Child 2", 2, null)
        val allChildren = listOf(bookMarkChild1, bookMarkChild2)
        val bookmarkNode = newBookmarkNode("Parent 1", 0, allChildren)
        val subsetToSubtract = setOf(bookMarkChild1)
        val expectedRemainingSubset = listOf(bookMarkChild2)
        val bookmarkNodeSubsetRemoved = bookmarkNode.minus(subsetToSubtract)
        assertEquals(expectedRemainingSubset, bookmarkNodeSubsetRemoved.children)
    }

    @Test
    fun `GIVEN a bookmark node with children WHEN minusing a set of all children THEN all children are removed and empty list remains`() {
        val bookMarkChild1 = newBookmarkNode("Child 1", 1, null)
        val bookMarkChild2 = newBookmarkNode("Child 2", 2, null)
        val allChildren = listOf(bookMarkChild1, bookMarkChild2)
        val bookmarkNode = newBookmarkNode("Parent 1", 0, allChildren)
        val setofAllChildren = setOf(bookMarkChild1, bookMarkChild2)
        val bookmarkNodeAllChildrenRemoved = bookmarkNode.minus(setofAllChildren)
        assertEquals(emptyList<BookmarkNode>(), bookmarkNodeAllChildrenRemoved.children)
    }

    @Test
    fun `GIVEN a bookmark node with children WHEN minusing a set of non-children THEN no children are removed`() {
        val bookMarkChild1 = newBookmarkNode("Child 1", 1, null)
        val bookMarkChild2 = newBookmarkNode("Child 2", 2, null)
        val allChildren = listOf(bookMarkChild1, bookMarkChild2)
        val bookMarkChild3 = newBookmarkNode("Child 3", 3, null)
        val setofNonChildren = setOf(bookMarkChild3)
        val bookmarkNode = newBookmarkNode("Parent 1", 0, allChildren)
        val bookmarkNodeNonChildrenRemoved = bookmarkNode.minus(setofNonChildren)
        assertEquals(allChildren, bookmarkNodeNonChildrenRemoved.children)
    }

    @Test
    fun `GIVEN a bookmark node with children WHEN minusing an empty set THEN no children are removed`() {
        val bookMarkChild1 = newBookmarkNode("Child 1", 1, null)
        val bookMarkChild2 = newBookmarkNode("Child 2", 2, null)
        val allChildren = listOf(bookMarkChild1, bookMarkChild2)
        val bookmarkNode = newBookmarkNode("Parent 1", 0, allChildren)
        val bookmarkNodeEmptySetRemoved = bookmarkNode.minus(emptySet())
        assertEquals(allChildren, bookmarkNodeEmptySetRemoved.children)
    }

    @Test
    fun `GIVEN a bookmark node with an empty list as children WHEN minusing a set of non-children from an empty parent THEN an empty list remains`() {
        val parentWithEmptyList = newBookmarkNode("Parent 1", 0, emptyList<BookmarkNode>())
        val bookMarkChild1 = newBookmarkNode("Child 1", 1, null)
        val bookMarkChild2 = newBookmarkNode("Child 2", 2, null)
        val setofAllChildren = setOf(bookMarkChild1, bookMarkChild2)
        val parentWithEmptyListNonChildRemoved = parentWithEmptyList.minus(setofAllChildren)
        assertEquals(emptyList<BookmarkNode>(), parentWithEmptyListNonChildRemoved.children)
    }

    @Test
    fun `GIVEN a bookmark node with null as children WHEN minusing a set of non-children from a parent with null children THEN null remains`() {
        val parentWithNullList = newBookmarkNode("Parent 1", 0, null)
        val bookMarkChild1 = newBookmarkNode("Child 1", 1, null)
        val bookMarkChild2 = newBookmarkNode("Child 2", 2, null)
        val setofAllChildren = setOf(bookMarkChild1, bookMarkChild2)
        val parentWithNullListNonChildRemoved = parentWithNullList.minus(setofAllChildren)
        assertEquals(null, parentWithNullListNonChildRemoved.children)
    }

    @Test
    fun `GIVEN a bookmark node with children WHEN minusing a sub-set of children THEN the rest of the parents object should remain the same`() {
        val bookMarkChild1 = newBookmarkNode("Child 1", 1, null)
        val bookMarkChild2 = newBookmarkNode("Child 2", 2, null)
        val allChildren = listOf(bookMarkChild1, bookMarkChild2)
        val bookmarkNode = newBookmarkNode("Parent 1", 0, allChildren)
        val subsetToSubtract = setOf(bookMarkChild1)
        val expectedRemainingSubset = listOf(bookMarkChild2)
        val resultBookmarkNode = bookmarkNode.minus(subsetToSubtract)

        // We're pinning children to the same value so we can compare the rest.
        val restOfResult = resultBookmarkNode.copy(children = expectedRemainingSubset)
        val restofOriginal = bookmarkNode.copy(children = expectedRemainingSubset)
        assertEquals(restOfResult, restofOriginal)
    }

    private fun newBookmarkNode(title: String, position: Int, children: List<BookmarkNode>?) = BookmarkNode(
            type = BookmarkNodeType.ITEM,
            guid = "11",
            parentGuid = "12",
            position = position,
            title = title,
            url = "www.mockurl.com",
            children = children
    )
}
