/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

/**
 * Interactor for the history screen
 * Provides implementations for the HistoryViewInteractor
 */
class HistoryInteractor(
    private val store: HistoryStore,
    private val openToBrowser: (item: HistoryItem) -> Unit,
    private val displayDeleteAll: () -> Unit,
    private val invalidateOptionsMenu: () -> Unit,
    private val deleteHistoryItems: (List<HistoryItem>) -> Unit
) : HistoryViewInteractor {
    override fun onHistoryItemOpened(item: HistoryItem) {
        openToBrowser(item)
    }

    override fun onEnterEditMode(selectedItem: HistoryItem) {
        store.dispatch(HistoryAction.EnterEditMode(selectedItem))
    }

    override fun onBackPressed() {
        store.dispatch(HistoryAction.ExitEditMode)
    }

    override fun onItemAddedForRemoval(item: HistoryItem) {
        store.dispatch(HistoryAction.AddItemForRemoval(item))
    }

    override fun onItemRemovedForRemoval(item: HistoryItem) {
        store.dispatch(HistoryAction.RemoveItemForRemoval(item))
    }

    override fun onModeSwitched() {
        invalidateOptionsMenu.invoke()
    }

    override fun onDeleteAll() {
        displayDeleteAll.invoke()
    }

    override fun onDeleteOne(item: HistoryItem) {
        deleteHistoryItems.invoke(listOf(item))
    }

    override fun onDeleteSome(items: List<HistoryItem>) {
        deleteHistoryItems.invoke(items)
    }
}
