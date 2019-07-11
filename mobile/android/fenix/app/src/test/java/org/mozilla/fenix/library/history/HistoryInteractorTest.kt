/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import io.mockk.mockk
import io.mockk.verify
import org.junit.Assert.assertEquals
import org.junit.Test

class HistoryInteractorTest {

    @Test
    fun onHistoryItemOpened() {
        var historyItemReceived: HistoryItem? = null
        val historyItem = HistoryItem(0, "title", "url", 0.toLong())
        val interactor = HistoryInteractor(
            mockk(),
            { historyItemReceived = it },
            mockk(),
            mockk(),
            mockk()
        )
        interactor.onHistoryItemOpened(historyItem)
        assertEquals(historyItem, historyItemReceived)
    }

    @Test
    fun onEnterEditMode() {
        val store: HistoryStore = mockk(relaxed = true)
        val newHistoryItem: HistoryItem = mockk(relaxed = true)
        val interactor =
            HistoryInteractor(store, mockk(), mockk(), mockk(), mockk())
        interactor.onEnterEditMode(newHistoryItem)
        verify { store.dispatch(HistoryAction.EnterEditMode(newHistoryItem)) }
    }

    @Test
    fun onBackPressed() {
        val store: HistoryStore = mockk(relaxed = true)
        val interactor =
            HistoryInteractor(store, mockk(), mockk(), mockk(), mockk())
        interactor.onBackPressed()
        verify { store.dispatch(HistoryAction.ExitEditMode) }
    }

    @Test
    fun onItemAddedForRemoval() {
        val store: HistoryStore = mockk(relaxed = true)
        val newHistoryItem: HistoryItem = mockk(relaxed = true)

        val interactor =
            HistoryInteractor(store, mockk(), mockk(), mockk(), mockk())
        interactor.onItemAddedForRemoval(newHistoryItem)
        verify { store.dispatch(HistoryAction.AddItemForRemoval(newHistoryItem)) }
    }

    @Test
    fun onItemRemovedForRemoval() {
        val store: HistoryStore = mockk(relaxed = true)
        val newHistoryItem: HistoryItem = mockk(relaxed = true)
        val interactor =
            HistoryInteractor(store, mockk(), mockk(), mockk(), mockk())
        interactor.onItemRemovedForRemoval(newHistoryItem)
        verify { store.dispatch(HistoryAction.RemoveItemForRemoval(newHistoryItem)) }
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
    fun onDeleteOne() {
        var itemsToDelete: List<HistoryItem>? = null
        val historyItem = HistoryItem(0, "title", "url", 0.toLong())
        val interactor =
            HistoryInteractor(
                mockk(),
                mockk(),
                mockk(),
                mockk(),
                { itemsToDelete = it }
            )
        interactor.onDeleteOne(historyItem)
        assertEquals(itemsToDelete, listOf(historyItem))
    }

    @Test
    fun onDeleteSome() {
        var itemsToDelete: List<HistoryItem>? = null
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
        interactor.onDeleteSome(listOf(historyItem, newHistoryItem))
        assertEquals(itemsToDelete, listOf(historyItem, newHistoryItem))
    }
}
