/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotSame
import org.junit.Test

class HistoryStoreTest {
    private val historyItem = HistoryItem(0, "title", "url", 0.toLong())
    private val newHistoryItem = HistoryItem(1, "title", "url", 0.toLong())

    @Test
    fun exitEditMode() = runBlocking {
        val initialState = oneItemEditState()
        val store = HistoryStore(initialState)

        store.dispatch(HistoryAction.ExitEditMode).join()
        assertNotSame(initialState, store.state)
        assertEquals(store.state.mode, HistoryState.Mode.Normal)
    }

    @Test
    fun itemAddedForRemoval() = runBlocking {
        val initialState = emptyDefaultState()
        val store = HistoryStore(initialState)

        store.dispatch(HistoryAction.AddItemForRemoval(newHistoryItem)).join()
        assertNotSame(initialState, store.state)
        assertEquals(
            store.state.mode,
            HistoryState.Mode.Editing(setOf(newHistoryItem))
        )
    }

    @Test
    fun removeItemForRemoval() = runBlocking {
        val initialState = twoItemEditState()
        val store = HistoryStore(initialState)

        store.dispatch(HistoryAction.RemoveItemForRemoval(newHistoryItem)).join()
        assertNotSame(initialState, store.state)
        assertEquals(store.state.mode, HistoryState.Mode.Editing(setOf(historyItem)))
    }

    private fun emptyDefaultState(): HistoryState = HistoryState(
        items = listOf(),
        mode = HistoryState.Mode.Normal
    )

    private fun oneItemEditState(): HistoryState = HistoryState(
        items = listOf(),
        mode = HistoryState.Mode.Editing(setOf(historyItem))
    )

    private fun twoItemEditState(): HistoryState = HistoryState(
        items = listOf(),
        mode = HistoryState.Mode.Editing(setOf(historyItem, newHistoryItem))
    )
}
