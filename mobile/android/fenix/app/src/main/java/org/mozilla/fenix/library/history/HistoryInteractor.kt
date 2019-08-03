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
    private val deleteHistoryItems: (Set<HistoryItem>) -> Unit
) : HistoryViewInteractor {
    override fun open(item: HistoryItem) {
        openToBrowser(item)
    }

    override fun select(item: HistoryItem) {
        store.dispatch(HistoryAction.AddItemForRemoval(item))
    }

    override fun deselect(item: HistoryItem) {
        store.dispatch(HistoryAction.RemoveItemForRemoval(item))
    }

    override fun onBackPressed(): Boolean {
        return if (store.state.mode is HistoryState.Mode.Editing) {
            store.dispatch(HistoryAction.ExitEditMode)
            true
        } else {
            false
        }
    }

    override fun onModeSwitched() {
        invalidateOptionsMenu.invoke()
    }

    override fun onDeleteAll() {
        displayDeleteAll.invoke()
    }

    override fun onDeleteSome(items: Set<HistoryItem>) {
        deleteHistoryItems.invoke(items)
    }
}
