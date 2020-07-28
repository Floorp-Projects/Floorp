/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.bookmarks

import kotlinx.coroutines.runBlocking
import mozilla.components.concept.storage.BookmarkNode
import mozilla.components.concept.storage.BookmarkNodeType
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test

class BookmarkFragmentStoreTest {

    @Test
    fun `change the tree of bookmarks starting from an empty tree`() = runBlocking {
        val initialState = BookmarkFragmentState(null)
        val store = BookmarkFragmentStore(initialState)

        assertEquals(store.state, BookmarkFragmentState(null, BookmarkFragmentState.Mode.Normal()))

        store.dispatch(BookmarkFragmentAction.Change(tree)).join()

        assertEquals(store.state.tree, tree)
        assertEquals(store.state.mode, initialState.mode)
    }

    @Test
    fun `change the tree of bookmarks starting from an existing tree`() = runBlocking {
        val initialState = BookmarkFragmentState(tree)
        val store = BookmarkFragmentStore(initialState)

        assertEquals(store.state, BookmarkFragmentState(tree, BookmarkFragmentState.Mode.Normal()))

        store.dispatch(BookmarkFragmentAction.Change(newTree)).join()

        assertEquals(store.state.tree, newTree)
        assertEquals(store.state.mode, initialState.mode)
    }

    @Test
    fun `changing the tree of bookmarks adds the tree to the visited nodes`() = runBlocking {
        val initialState = BookmarkFragmentState(null)
        val store = BookmarkFragmentStore(initialState)

        store.dispatch(BookmarkFragmentAction.Change(tree)).join()
        store.dispatch(BookmarkFragmentAction.Change(subfolder)).join()

        assertEquals(listOf(tree.guid, subfolder.guid), store.state.guidBackstack)
    }

    @Test
    fun `changing to a node that is in the backstack removes backstack items after that node`() = runBlocking {
        val initialState = BookmarkFragmentState(
            null,
            guidBackstack = listOf(tree.guid, subfolder.guid, item.guid)
        )
        val store = BookmarkFragmentStore(initialState)

        store.dispatch(BookmarkFragmentAction.Change(tree)).join()

        assertEquals(listOf(tree.guid), store.state.guidBackstack)
    }

    @Test
    fun `change the tree of bookmarks to the same value`() = runBlocking {
        val initialState = BookmarkFragmentState(tree)
        val store = BookmarkFragmentStore(initialState)

        assertEquals(store.state, BookmarkFragmentState(tree, BookmarkFragmentState.Mode.Normal()))

        store.dispatch(BookmarkFragmentAction.Change(tree)).join()

        assertEquals(store.state.tree, initialState.tree)
        assertEquals(store.state.mode, initialState.mode)
    }

    @Test
    fun `ensure selected items remain selected after a tree change`() = runBlocking {
        val initialState = BookmarkFragmentState(
            tree,
            BookmarkFragmentState.Mode.Selecting(setOf(item, subfolder)),
            isLoading = false,
            isSwipeToRefreshEnabled = false
        )
        val store = BookmarkFragmentStore(initialState)

        store.dispatch(BookmarkFragmentAction.Change(newTree)).join()

        assertEquals(
            store.state,
            BookmarkFragmentState(
                newTree,
                BookmarkFragmentState.Mode.Selecting(setOf(subfolder)),
                guidBackstack = listOf(tree.guid),
                isLoading = false,
                isSwipeToRefreshEnabled = false
            )
        )
    }

    @Test
    fun `select and deselect a single bookmark changes the mode and swipe to refresh state`() = runBlocking {
        val initialState = BookmarkFragmentState(tree)
        val store = BookmarkFragmentStore(initialState)

        store.dispatch(BookmarkFragmentAction.Select(childItem)).join()

        assertEquals(
            store.state,
            BookmarkFragmentState(
                tree,
                BookmarkFragmentState.Mode.Selecting(setOf(childItem)),
                isSwipeToRefreshEnabled = false
            )
        )

        store.dispatch(BookmarkFragmentAction.Deselect(childItem)).join()

        assertEquals(
            store.state,
            BookmarkFragmentState(
                tree,
                BookmarkFragmentState.Mode.Normal(),
                isSwipeToRefreshEnabled = true
            )
        )
    }

    @Test
    fun `selecting the same item twice does nothing`() = runBlocking {
        val initialState = BookmarkFragmentState(
            tree,
            BookmarkFragmentState.Mode.Selecting(setOf(item, subfolder)),
            isSwipeToRefreshEnabled = false
        )
        val store = BookmarkFragmentStore(initialState)

        store.dispatch(BookmarkFragmentAction.Select(item)).join()

        assertSame(initialState, store.state)
    }

    @Test
    fun `deselecting an unselected bookmark does nothing`() = runBlocking {
        val initialState = BookmarkFragmentState(
            tree,
            BookmarkFragmentState.Mode.Selecting(setOf(childItem)),
            isSwipeToRefreshEnabled = false
        )
        val store = BookmarkFragmentStore(initialState)

        store.dispatch(BookmarkFragmentAction.Deselect(item)).join()

        assertSame(initialState, store.state)
    }

    @Test
    fun `deselecting while not in selecting mode does nothing`() = runBlocking {
        val initialState = BookmarkFragmentState(tree, BookmarkFragmentState.Mode.Normal())
        val store = BookmarkFragmentStore(initialState)

        store.dispatch(BookmarkFragmentAction.Deselect(item)).join()

        assertSame(initialState, store.state)
    }

    @Test
    fun `deselect all bookmarks changes the mode and updates swipe to refresh state`() =
        runBlocking {
            val initialState = BookmarkFragmentState(
                tree,
                BookmarkFragmentState.Mode.Selecting(setOf(item, childItem)),
                isSwipeToRefreshEnabled = false
            )
            val store = BookmarkFragmentStore(initialState)

            store.dispatch(BookmarkFragmentAction.DeselectAll).join()

            assertEquals(
                store.state,
                initialState.copy(
                    mode = BookmarkFragmentState.Mode.Normal(),
                    isSwipeToRefreshEnabled = true
                )
            )
        }

    @Test
    fun `deselect all bookmarks when none are selected`() = runBlocking {
        val initialState = BookmarkFragmentState(tree, BookmarkFragmentState.Mode.Normal())
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
            assertEquals(tree, newTree)
            assertEquals(mode, BookmarkFragmentState.Mode.Normal())
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

    @Test
    fun `switching to Desktop Bookmarks folder sets showMenu state to false`() = runBlocking {
        val initialState = BookmarkFragmentState(tree)
        val store = BookmarkFragmentStore(initialState)

        store.dispatch(BookmarkFragmentAction.Change(rootFolder)).join()

        assertEquals(store.state.tree, rootFolder)
        assertEquals(store.state.mode, BookmarkFragmentState.Mode.Normal(false))
    }

    @Test
    fun `changing the tree or deselecting in Syncing mode should stay in Syncing mode`() = runBlocking {
        val initialState = BookmarkFragmentState(tree)
        val store = BookmarkFragmentStore(initialState)

        store.dispatch(BookmarkFragmentAction.StartSync).join()
        store.dispatch(BookmarkFragmentAction.Change(childItem))
        assertEquals(BookmarkFragmentState.Mode.Syncing, store.state.mode)

        store.dispatch(BookmarkFragmentAction.DeselectAll).join()
        assertEquals(BookmarkFragmentState.Mode.Syncing, store.state.mode)
    }

    @Test
    fun `enabling swipe to refresh in Normal mode works`() = runBlocking {
        val initialState = BookmarkFragmentState(
            tree,
            BookmarkFragmentState.Mode.Normal(),
            isSwipeToRefreshEnabled = false
        )
        val store = BookmarkFragmentStore(initialState)

        store.dispatch(BookmarkFragmentAction.SwipeRefreshAvailabilityChanged(true)).join()
        assertEquals(true, store.state.isSwipeToRefreshEnabled)
    }

    @Test
    fun `enabling swipe to refresh in Syncing mode works`() = runBlocking {
        val initialState = BookmarkFragmentState(
            tree,
            BookmarkFragmentState.Mode.Syncing,
            isSwipeToRefreshEnabled = false
        )
        val store = BookmarkFragmentStore(initialState)

        store.dispatch(BookmarkFragmentAction.SwipeRefreshAvailabilityChanged(true)).join()
        assertEquals(true, store.state.isSwipeToRefreshEnabled)
    }

    @Test
    fun `enabling swipe to refresh in Selecting mode does not work`() = runBlocking {
        val initialState = BookmarkFragmentState(
            tree,
            BookmarkFragmentState.Mode.Selecting(emptySet()),
            isSwipeToRefreshEnabled = false
        )
        val store = BookmarkFragmentStore(initialState)

        store.dispatch(BookmarkFragmentAction.SwipeRefreshAvailabilityChanged(true)).join()
        assertEquals(false, store.state.isSwipeToRefreshEnabled)
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
    private val rootFolder = BookmarkNode(
        BookmarkNodeType.FOLDER,
        "root________",
        null,
        0,
        "Desktop Bookmarks",
        null,
        listOf(subfolder)
    )
}
