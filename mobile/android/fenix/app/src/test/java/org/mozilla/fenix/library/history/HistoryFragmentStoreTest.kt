/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import kotlinx.coroutines.test.runTest
import mozilla.components.support.test.ext.joinBlocking
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
            HistoryFragmentState.Mode.Editing(setOf(newHistoryItem)),
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
            isDeletingItems = false,
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

    @Test
    fun `GIVEN items have been selected WHEN selected item is clicked THEN item is unselected`() = runTest {
        val store = HistoryFragmentStore(twoItemEditState())

        store.dispatch(HistoryFragmentAction.HistoryItemClicked(historyItem)).joinBlocking()

        assertEquals(1, store.state.mode.selectedItems.size)
        assertEquals(newHistoryItem, store.state.mode.selectedItems.first())
    }

    @Test
    fun `GIVEN items have been selected WHEN unselected item is clicked THEN item is selected`() {
        val initialState = oneItemEditState().copy(items = listOf(newHistoryItem))
        val store = HistoryFragmentStore(initialState)

        store.dispatch(HistoryFragmentAction.HistoryItemClicked(newHistoryItem)).joinBlocking()

        assertEquals(2, store.state.mode.selectedItems.size)
        assertTrue(store.state.mode.selectedItems.contains(newHistoryItem))
    }

    @Test
    fun `GIVEN items have been selected WHEN last selected item is clicked THEN editing mode is exited`() {
        val store = HistoryFragmentStore(oneItemEditState())

        store.dispatch(HistoryFragmentAction.HistoryItemClicked(historyItem)).joinBlocking()

        assertEquals(0, store.state.mode.selectedItems.size)
        assertTrue(store.state.mode is HistoryFragmentState.Mode.Normal)
    }

    @Test
    fun `GIVEN items have not been selected WHEN item is clicked THEN state is unchanged`() {
        val store = HistoryFragmentStore(emptyDefaultState())

        store.dispatch(HistoryFragmentAction.HistoryItemClicked(historyItem)).joinBlocking()

        assertEquals(0, store.state.mode.selectedItems.size)
        assertTrue(store.state.mode is HistoryFragmentState.Mode.Normal)
    }

    @Test
    fun `GIVEN mode is syncing WHEN item is clicked THEN state is unchanged`() {
        val store = HistoryFragmentStore(emptyDefaultState().copy(mode = HistoryFragmentState.Mode.Syncing))

        store.dispatch(HistoryFragmentAction.HistoryItemClicked(historyItem)).joinBlocking()

        assertEquals(0, store.state.mode.selectedItems.size)
        assertTrue(store.state.mode is HistoryFragmentState.Mode.Syncing)
    }

    @Test
    fun `GIVEN mode is syncing WHEN item is long-clicked THEN state is unchanged`() {
        val store = HistoryFragmentStore(emptyDefaultState().copy(mode = HistoryFragmentState.Mode.Syncing))

        store.dispatch(HistoryFragmentAction.HistoryItemLongClicked(historyItem)).joinBlocking()

        assertEquals(0, store.state.mode.selectedItems.size)
        assertTrue(store.state.mode is HistoryFragmentState.Mode.Syncing)
    }

    @Test
    fun `GIVEN mode is not syncing WHEN item is long-clicked THEN mode becomes editing`() {
        val store = HistoryFragmentStore(oneItemEditState())

        store.dispatch(HistoryFragmentAction.HistoryItemLongClicked(newHistoryItem)).joinBlocking()

        assertEquals(2, store.state.mode.selectedItems.size)
        assertTrue(store.state.mode.selectedItems.contains(newHistoryItem))
    }

    @Test
    fun `GIVEN mode is not syncing WHEN item is long-clicked THEN item is selected`() {
        val store = HistoryFragmentStore(emptyDefaultState())

        store.dispatch(HistoryFragmentAction.HistoryItemLongClicked(historyItem)).joinBlocking()

        assertEquals(1, store.state.mode.selectedItems.size)
        assertTrue(store.state.mode.selectedItems.contains(historyItem))
    }

    @Test
    fun `GIVEN mode is editing WHEN back pressed THEN mode becomes normal`() {
        val store = HistoryFragmentStore(
            emptyDefaultState().copy(
                mode = HistoryFragmentState.Mode.Editing(
                    setOf(),
                ),
            ),
        )

        store.dispatch(HistoryFragmentAction.BackPressed).joinBlocking()

        assertEquals(HistoryFragmentState.Mode.Normal, store.state.mode)
    }

    @Test
    fun `GIVEN mode is not editing WHEN back pressed THEN does not change`() {
        val store = HistoryFragmentStore(emptyDefaultState().copy(mode = HistoryFragmentState.Mode.Syncing))

        store.dispatch(HistoryFragmentAction.BackPressed).joinBlocking()

        assertEquals(HistoryFragmentState.Mode.Syncing, store.state.mode)
    }

    private fun emptyDefaultState(): HistoryFragmentState = HistoryFragmentState(
        items = listOf(),
        mode = HistoryFragmentState.Mode.Normal,
        pendingDeletionItems = emptySet(),
        isEmpty = false,
        isDeletingItems = false,
    )

    private fun oneItemEditState(): HistoryFragmentState = HistoryFragmentState(
        items = listOf(),
        mode = HistoryFragmentState.Mode.Editing(setOf(historyItem)),
        pendingDeletionItems = emptySet(),
        isEmpty = false,
        isDeletingItems = false,
    )

    private fun twoItemEditState(): HistoryFragmentState = HistoryFragmentState(
        items = listOf(),
        mode = HistoryFragmentState.Mode.Editing(setOf(historyItem, newHistoryItem)),
        pendingDeletionItems = emptySet(),
        isEmpty = false,
        isDeletingItems = false,
    )
}
