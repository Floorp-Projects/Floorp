/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import kotlinx.coroutines.test.runTest
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotSame
import org.junit.Assert.assertTrue
import org.junit.Test

class HistoryFragmentStoreTest {
    private val historyItem = History.Regular(0, "title", "url", 0.toLong(), HistoryItemTimeGroup.timeGroupForTimestamp(0))
    private val newHistoryItem = History.Regular(1, "title", "url", 0.toLong(), HistoryItemTimeGroup.timeGroupForTimestamp(0))
    private val pendingDeletionItem = historyItem.toPendingDeletionHistory()

    @Test
    fun exitEditMode() = runTest {
        val initialState = oneItemEditState()
        val store = HistoryFragmentStore(initialState)

        store.dispatch(HistoryFragmentAction.ExitEditMode).join()
        assertNotSame(initialState, store.state)
        assertEquals(store.state.mode, HistoryFragmentState.Mode.Normal)
    }

    @Test
    fun itemAddedForRemoval() = runTest {
        val initialState = emptyDefaultState()
        val store = HistoryFragmentStore(initialState)

        store.dispatch(HistoryFragmentAction.AddItemForRemoval(newHistoryItem)).join()
        assertNotSame(initialState, store.state)
        assertEquals(
            store.state.mode,
            HistoryFragmentState.Mode.Editing(setOf(newHistoryItem))
        )
    }

    @Test
    fun removeItemForRemoval() = runTest {
        val initialState = twoItemEditState()
        val store = HistoryFragmentStore(initialState)

        store.dispatch(HistoryFragmentAction.RemoveItemForRemoval(newHistoryItem)).join()
        assertNotSame(initialState, store.state)
        assertEquals(store.state.mode, HistoryFragmentState.Mode.Editing(setOf(historyItem)))
    }

    @Test
    fun startSync() = runTest {
        val initialState = emptyDefaultState()
        val store = HistoryFragmentStore(initialState)

        store.dispatch(HistoryFragmentAction.StartSync).join()
        assertNotSame(initialState, store.state)
        assertEquals(HistoryFragmentState.Mode.Syncing, store.state.mode)
    }

    @Test
    fun finishSync() = runTest {
        val initialState = HistoryFragmentState(
            items = listOf(),
            mode = HistoryFragmentState.Mode.Syncing,
            pendingDeletionItems = emptySet(),
            isEmpty = false,
            isDeletingItems = false
        )
        val store = HistoryFragmentStore(initialState)

        store.dispatch(HistoryFragmentAction.FinishSync).join()
        assertNotSame(initialState, store.state)
        assertEquals(HistoryFragmentState.Mode.Normal, store.state.mode)
    }

    @Test
    fun changeEmptyState() = runTest {
        val initialState = emptyDefaultState()
        val store = HistoryFragmentStore(initialState)

        store.dispatch(HistoryFragmentAction.ChangeEmptyState(true)).join()
        assertNotSame(initialState, store.state)
        assertTrue(store.state.isEmpty)

        store.dispatch(HistoryFragmentAction.ChangeEmptyState(false)).join()
        assertNotSame(initialState, store.state)
        assertFalse(store.state.isEmpty)
    }

    @Test
    fun updatePendingDeletionItems() = runTest {
        val initialState = emptyDefaultState()
        val store = HistoryFragmentStore(initialState)

        store.dispatch(HistoryFragmentAction.UpdatePendingDeletionItems(setOf(pendingDeletionItem))).join()
        assertNotSame(initialState, store.state)
        assertEquals(setOf(pendingDeletionItem), store.state.pendingDeletionItems)

        store.dispatch(HistoryFragmentAction.UpdatePendingDeletionItems(emptySet())).join()
        assertNotSame(initialState, store.state)
        assertEquals(emptySet<PendingDeletionHistory>(), store.state.pendingDeletionItems)
    }

    private fun emptyDefaultState(): HistoryFragmentState = HistoryFragmentState(
        items = listOf(),
        mode = HistoryFragmentState.Mode.Normal,
        pendingDeletionItems = emptySet(),
        isEmpty = false,
        isDeletingItems = false
    )

    private fun oneItemEditState(): HistoryFragmentState = HistoryFragmentState(
        items = listOf(),
        mode = HistoryFragmentState.Mode.Editing(setOf(historyItem)),
        pendingDeletionItems = emptySet(),
        isEmpty = false,
        isDeletingItems = false
    )

    private fun twoItemEditState(): HistoryFragmentState = HistoryFragmentState(
        items = listOf(),
        mode = HistoryFragmentState.Mode.Editing(setOf(historyItem, newHistoryItem)),
        pendingDeletionItems = emptySet(),
        isEmpty = false,
        isDeletingItems = false
    )
}
