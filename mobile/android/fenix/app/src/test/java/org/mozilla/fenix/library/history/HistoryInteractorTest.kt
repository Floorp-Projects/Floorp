/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class HistoryInteractorTest {

    @Test
    fun onPressHistoryItemInNormalMode() {
        var historyItemReceived: HistoryItem? = null
        val historyItem = HistoryItem(0, "title", "url", 0.toLong())
        val store: HistoryStore = mockk()
        val state: HistoryState = mockk()
        every { store.state } returns state
        every { state.mode } returns HistoryState.Mode.Normal

        val interactor = HistoryInteractor(
            store,
            { historyItemReceived = it },
            mockk(),
            mockk(),
            mockk()
        )

        interactor.open(historyItem)
        assertEquals(historyItem, historyItemReceived)
    }

    @Test
    fun onPressHistoryItemInEditMode() {
        val historyItem = HistoryItem(0, "title", "url", 0.toLong())
        val store: HistoryStore = mockk(relaxed = true)
        val state: HistoryState = mockk()
        every { store.state } returns state
        every { state.mode } returns HistoryState.Mode.Editing(setOf())

        val interactor = HistoryInteractor(
            store,
            { },
            mockk(),
            mockk(),
            mockk()
        )

        interactor.select(historyItem)

        verify {
            store.dispatch(HistoryAction.AddItemForRemoval(historyItem))
        }
    }

    @Test
    fun onPressSelectedHistoryItemInEditMode() {
        val historyItem = HistoryItem(0, "title", "url", 0.toLong())
        val store: HistoryStore = mockk(relaxed = true)
        val state: HistoryState = mockk()
        every { store.state } returns state
        every { state.mode } returns HistoryState.Mode.Editing(setOf(historyItem))

        val interactor = HistoryInteractor(
            store,
            { },
            mockk(),
            mockk(),
            mockk()
        )

        interactor.deselect(historyItem)

        verify {
            store.dispatch(HistoryAction.RemoveItemForRemoval(historyItem))
        }
    }

    @Test
    fun onBackPressedInNormalMode() {
        val store: HistoryStore = mockk(relaxed = true)
        val state: HistoryState = mockk()
        every { store.state } returns state
        every { state.mode } returns HistoryState.Mode.Normal

        val interactor = HistoryInteractor(store, mockk(), mockk(), mockk(), mockk())
        assertFalse(interactor.onBackPressed())
    }

    @Test
    fun onBackPressedInEditMode() {
        val store: HistoryStore = mockk(relaxed = true)
        val state: HistoryState = mockk()
        every { store.state } returns state
        every { state.mode } returns HistoryState.Mode.Editing(setOf())

        val interactor = HistoryInteractor(store, mockk(), mockk(), mockk(), mockk())
        assertTrue(interactor.onBackPressed())

        verify {
            store.dispatch(HistoryAction.ExitEditMode)
        }
    }

    @Test
    fun onModeSwitched() {
        var menuInvalidated = false
        val interactor = HistoryInteractor(
            mockk(),
            mockk(),
            mockk(),
            { menuInvalidated = true },
            mockk()
        )
        interactor.onModeSwitched()
        assertEquals(true, menuInvalidated)
    }

    @Test
    fun onDeleteAll() {
        var deleteAllDialogShown = false
        val interactor = HistoryInteractor(
            mockk(),
            mockk(),
            { deleteAllDialogShown = true },
            mockk(),
            mockk()
        )
        interactor.onDeleteAll()
        assertEquals(true, deleteAllDialogShown)
    }

    @Test
    fun onDeleteSome() {
        var itemsToDelete: Set<HistoryItem>? = null
        val historyItem = HistoryItem(0, "title", "url", 0.toLong())
        val newHistoryItem = HistoryItem(1, "title", "url", 0.toLong())
        val interactor =
            HistoryInteractor(
                mockk(),
                mockk(),
                mockk(),
                mockk(),
                { itemsToDelete = it }
            )
        interactor.onDeleteSome(setOf(historyItem, newHistoryItem))
        assertEquals(itemsToDelete, setOf(historyItem, newHistoryItem))
    }
}
