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
    override fun onItemPress(item: HistoryItem) {
        val mode = store.state.mode
        when (mode) {
            is HistoryState.Mode.Normal -> openToBrowser(item)
            is HistoryState.Mode.Editing -> {
                val isSelected = mode.selectedItems.contains(item)

                if (isSelected) {
                    store.dispatch(HistoryAction.RemoveItemForRemoval(item))
                } else {
                    store.dispatch(HistoryAction.AddItemForRemoval(item))
                }
            }
        }
    }

    override fun onItemLongPress(item: HistoryItem) {
        val isSelected = (store.state.mode as? HistoryState.Mode.Editing)?.let {
            it.selectedItems.contains(item)
        } ?: false

        if (isSelected) {
            store.dispatch(HistoryAction.RemoveItemForRemoval(item))
        } else {
            store.dispatch(HistoryAction.AddItemForRemoval(item))
        }
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

    override fun onDeleteOne(item: HistoryItem) {
        deleteHistoryItems.invoke(setOf(item))
    }

    override fun onDeleteSome(items: Set<HistoryItem>) {
        deleteHistoryItems.invoke(items)
    }
}
