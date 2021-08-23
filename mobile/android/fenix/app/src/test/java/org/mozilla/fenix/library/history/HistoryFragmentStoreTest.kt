/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotSame
import org.junit.Test

class HistoryFragmentStoreTest {
    private val historyItem = History.Regular(0, "title", "url", 0.toLong())
    private val newHistoryItem = History.Regular(1, "title", "url", 0.toLong())

    @Test
    fun exitEditMode() = runBlocking {
        val initialState = oneItemEditState()
        val store = HistoryFragmentStore(initialState)

        store.dispatch(HistoryFragmentAction.ExitEditMode).join()
        assertNotSame(initialState, store.state)
        assertEquals(store.state.mode, HistoryFragmentState.Mode.Normal)
    }

    @Test
    fun itemAddedForRemoval() = runBlocking {
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
    fun removeItemForRemoval() = runBlocking {
        val initialState = twoItemEditState()
        val store = HistoryFragmentStore(initialState)

        store.dispatch(HistoryFragmentAction.RemoveItemForRemoval(newHistoryItem)).join()
        assertNotSame(initialState, store.state)
        assertEquals(store.state.mode, HistoryFragmentState.Mode.Editing(setOf(historyItem)))
    }

    @Test
    fun startSync() = runBlocking {
        val initialState = emptyDefaultState()
        val store = HistoryFragmentStore(initialState)

        store.dispatch(HistoryFragmentAction.StartSync).join()
        assertNotSame(initialState, store.state)
        assertEquals(HistoryFragmentState.Mode.Syncing, store.state.mode)
    }

    @Test
    fun finishSync() = runBlocking {
        val initialState = HistoryFragmentState(
            items = listOf(),
            mode = HistoryFragmentState.Mode.Syncing,
            pendingDeletionIds = emptySet(),
            isDeletingItems = false
        )
        val store = HistoryFragmentStore(initialState)

        store.dispatch(HistoryFragmentAction.FinishSync).join()
        assertNotSame(initialState, store.state)
        assertEquals(HistoryFragmentState.Mode.Normal, store.state.mode)
    }

    private fun emptyDefaultState(): HistoryFragmentState = HistoryFragmentState(
        items = listOf(),
        mode = HistoryFragmentState.Mode.Normal,
        pendingDeletionIds = emptySet(),
        isDeletingItems = false
    )

    private fun oneItemEditState(): HistoryFragmentState = HistoryFragmentState(
        items = listOf(),
        mode = HistoryFragmentState.Mode.Editing(setOf(historyItem)),
        pendingDeletionIds = emptySet(),
        isDeletingItems = false
    )

    private fun twoItemEditState(): HistoryFragmentState = HistoryFragmentState(
        items = listOf(),
        mode = HistoryFragmentState.Mode.Editing(setOf(historyItem, newHistoryItem)),
        pendingDeletionIds = emptySet(),
        isDeletingItems = false
    )
}
