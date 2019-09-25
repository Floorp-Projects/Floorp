/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import assertk.assertThat
import assertk.assertions.isEqualTo
import kotlinx.coroutines.runBlocking
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import org.junit.Assert.assertFalse
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test

class BookmarkFragmentStoreTest {

    @Test
    fun `change the tree of bookmarks starting from an empty tree`() = runBlocking {
        val initialState = BookmarkFragmentState(null)
        val store = BookmarkFragmentStore(initialState)

        assertThat(BookmarkFragmentState(null, BookmarkFragmentState.Mode.Normal)).isEqualTo(store.state)

        store.dispatch(BookmarkFragmentAction.Change(tree)).join()

        assertThat(tree).isEqualTo(store.state.tree)
        assertThat(initialState.mode).isEqualTo(store.state.mode)
    }

    @Test
    fun `change the tree of bookmarks starting from an existing tree`() = runBlocking {
        val initialState = BookmarkFragmentState(tree)
        val store = BookmarkFragmentStore(initialState)

        assertThat(BookmarkFragmentState(tree, BookmarkFragmentState.Mode.Normal)).isEqualTo(store.state)

        store.dispatch(BookmarkFragmentAction.Change(newTree)).join()

        assertThat(newTree).isEqualTo(store.state.tree)
        assertThat(initialState.mode).isEqualTo(store.state.mode)
    }

    @Test
    fun `change the tree of bookmarks to the same value`() = runBlocking {
        val initialState = BookmarkFragmentState(tree)
        val store = BookmarkFragmentStore(initialState)

        assertThat(BookmarkFragmentState(tree, BookmarkFragmentState.Mode.Normal)).isEqualTo(store.state)

        store.dispatch(BookmarkFragmentAction.Change(tree)).join()

        assertThat(initialState.tree).isEqualTo(store.state.tree)
        assertThat(initialState.mode).isEqualTo(store.state.mode)
    }

    @Test
    fun `ensure selected items remain selected after a tree change`() = runBlocking {
        val initialState = BookmarkFragmentState(tree, BookmarkFragmentState.Mode.Selecting(setOf(item, subfolder)))
        val store = BookmarkFragmentStore(initialState)

        store.dispatch(BookmarkFragmentAction.Change(newTree)).join()

        assertThat(newTree).isEqualTo(store.state.tree)
        assertThat(BookmarkFragmentState.Mode.Selecting(setOf(subfolder))).isEqualTo(store.state.mode)
    }

    @Test
    fun `select and deselect bookmarks changes the mode`() = runBlocking {
        val initialState = BookmarkFragmentState(tree)
        val store = BookmarkFragmentStore(initialState)

        store.dispatch(BookmarkFragmentAction.Select(childItem)).join()

        assertThat(BookmarkFragmentState(tree, BookmarkFragmentState.Mode.Selecting(setOf(childItem)))).isEqualTo(store.state)

        store.dispatch(BookmarkFragmentAction.Deselect(childItem)).join()

        assertThat(BookmarkFragmentState(tree, BookmarkFragmentState.Mode.Normal)).isEqualTo(store.state)
    }

    @Test
    fun `selecting the same item twice does nothing`() = runBlocking {
        val initialState = BookmarkFragmentState(tree, BookmarkFragmentState.Mode.Selecting(setOf(item, subfolder)))
        val store = BookmarkFragmentStore(initialState)

        store.dispatch(BookmarkFragmentAction.Select(item)).join()

        assertSame(initialState, store.state)
    }

    @Test
    fun `deselecting an unselected bookmark does nothing`() = runBlocking {
        val initialState = BookmarkFragmentState(tree, BookmarkFragmentState.Mode.Selecting(setOf(childItem)))
        val store = BookmarkFragmentStore(initialState)

        store.dispatch(BookmarkFragmentAction.Deselect(item)).join()

        assertSame(initialState, store.state)
    }

    @Test
    fun `deselecting while not in selecting mode does nothing`() = runBlocking {
        val initialState = BookmarkFragmentState(tree, BookmarkFragmentState.Mode.Normal)
        val store = BookmarkFragmentStore(initialState)

        store.dispatch(BookmarkFragmentAction.Deselect(item)).join()

        assertSame(initialState, store.state)
    }

    @Test
    fun `deselect all bookmarks changes the mode`() = runBlocking {
        val initialState = BookmarkFragmentState(tree, BookmarkFragmentState.Mode.Selecting(setOf(item, childItem)))
        val store = BookmarkFragmentStore(initialState)

        store.dispatch(BookmarkFragmentAction.DeselectAll).join()

        assertThat(initialState.copy(mode = BookmarkFragmentState.Mode.Normal)).isEqualTo(store.state)
    }

    @Test
    fun `deselect all bookmarks when none are selected`() = runBlocking {
        val initialState = BookmarkFragmentState(tree, BookmarkFragmentState.Mode.Normal)
        val store = BookmarkFragmentStore(initialState)

        store.dispatch(BookmarkFragmentAction.DeselectAll)

        assertSame(initialState, store.state)
    }

    @Test
    fun `deleting bookmarks changes the mode`() = runBlocking {
        val initialState = BookmarkFragmentState(tree, BookmarkFragmentState.Mode.Selecting(setOf(item, childItem)))
        val store = BookmarkFragmentStore(initialState)

        store.dispatch(BookmarkFragmentAction.Change(newTree)).join()

        store.state.run {
            assertThat(newTree).isEqualTo(tree)
            assertThat(BookmarkFragmentState.Mode.Normal).isEqualTo(mode)
        }
    }

    @Test
    fun `selecting and deselecting bookmarks does not affect loading state`() = runBlocking {
        val initialState = BookmarkFragmentState(tree, isLoading = true)
        val store = BookmarkFragmentStore(initialState)

        store.dispatch(BookmarkFragmentAction.Select(newTree)).join()
        assertTrue(store.state.isLoading)

        store.dispatch(BookmarkFragmentAction.Deselect(newTree)).join()
        assertTrue(store.state.isLoading)

        store.dispatch(BookmarkFragmentAction.DeselectAll).join()
        assertTrue(store.state.isLoading)
    }

    @Test
    fun `changing bookmarks disables loading state`() = runBlocking {
        val initialState = BookmarkFragmentState(tree, isLoading = true)
        val store = BookmarkFragmentStore(initialState)

        store.dispatch(BookmarkFragmentAction.Change(newTree)).join()
        assertFalse(store.state.isLoading)
    }

    private val item = BookmarkNode(BookmarkNodeType.ITEM, "456", "123", 0, "Mozilla", "http://mozilla.org", null)
    private val separator = BookmarkNode(BookmarkNodeType.SEPARATOR, "789", "123", 1, null, null, null)
    private val subfolder = BookmarkNode(BookmarkNodeType.FOLDER, "987", "123", 0, "Subfolder", null, listOf())
    private val childItem = BookmarkNode(
        BookmarkNodeType.ITEM,
        "987",
        "123",
        2,
        "Firefox",
        "https://www.mozilla.org/en-US/firefox/",
        null
    )
    private val tree = BookmarkNode(
        BookmarkNodeType.FOLDER, "123", null, 0, "Mobile", null, listOf(item, separator, childItem, subfolder)
    )
    private val newTree = BookmarkNode(
        BookmarkNodeType.FOLDER,
        "123",
        null,
        0,
        "Mobile",
        null,
        listOf(separator, subfolder)
    )
}
