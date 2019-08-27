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
import org.junit.Before
import org.junit.Test

class HistoryControllerTest {

    private val historyItem = HistoryItem(0, "title", "url", 0.toLong())
    private val store: HistoryStore = mockk(relaxed = true)
    private val state: HistoryState = mockk(relaxed = true)

    @Before
    fun setUp() {
        every { store.state } returns state
    }

    @Test
    fun onPressHistoryItemInNormalMode() {
        var historyItemReceived: HistoryItem? = null

        every { state.mode } returns HistoryState.Mode.Normal

        val controller = DefaultHistoryController(
            store,
            { historyItemReceived = it },
            mockk(),
            mockk(),
            mockk()
        )

        controller.handleOpen(historyItem)
        assertEquals(historyItem, historyItemReceived)
    }

    @Test
    fun onPressHistoryItemInEditMode() {
        every { state.mode } returns HistoryState.Mode.Editing(setOf())

        val controller = DefaultHistoryController(
            store,
            { },
            mockk(),
            mockk(),
            mockk()
        )

        controller.handleSelect(historyItem)

        verify {
            store.dispatch(HistoryAction.AddItemForRemoval(historyItem))
        }
    }

    @Test
    fun onPressSelectedHistoryItemInEditMode() {
        every { state.mode } returns HistoryState.Mode.Editing(setOf(historyItem))

        val controller = DefaultHistoryController(
            store,
            { },
            mockk(),
            mockk(),
            mockk()
        )

        controller.handleDeselect(historyItem)

        verify {
            store.dispatch(HistoryAction.RemoveItemForRemoval(historyItem))
        }
    }

    @Test
    fun onBackPressedInNormalMode() {
        every { state.mode } returns HistoryState.Mode.Normal

        val controller = DefaultHistoryController(store, mockk(), mockk(), mockk(), mockk())
        assertFalse(controller.handleBackPressed())
    }

    @Test
    fun onBackPressedInEditMode() {
        every { state.mode } returns HistoryState.Mode.Editing(setOf())

        val controller = DefaultHistoryController(store, mockk(), mockk(), mockk(), mockk())
        assertTrue(controller.handleBackPressed())

        verify {
            store.dispatch(HistoryAction.ExitEditMode)
        }
    }

    @Test
    fun onModeSwitched() {
        var menuInvalidated = false
        val controller = DefaultHistoryController(
            mockk(),
            mockk(),
            mockk(),
            { menuInvalidated = true },
            mockk()
        )
        controller.handleModeSwitched()
        assertEquals(true, menuInvalidated)
    }

    @Test
    fun onDeleteAll() {
        var deleteAllDialogShown = false
        val controller = DefaultHistoryController(
            mockk(),
            mockk(),
            { deleteAllDialogShown = true },
            mockk(),
            mockk()
        )
        controller.handleDeleteAll()
        assertEquals(true, deleteAllDialogShown)
    }

    @Test
    fun onDeleteSome() {
        var itemsToDelete: Set<HistoryItem>? = null
        val historyItem = HistoryItem(0, "title", "url", 0.toLong())
        val newHistoryItem = HistoryItem(1, "title", "url", 0.toLong())
        val controller = DefaultHistoryController(
                mockk(),
                mockk(),
                mockk(),
                mockk(),
                { itemsToDelete = it }
            )
        controller.handleDeleteSome(setOf(historyItem, newHistoryItem))
        assertEquals(itemsToDelete, setOf(historyItem, newHistoryItem))
    }
}
