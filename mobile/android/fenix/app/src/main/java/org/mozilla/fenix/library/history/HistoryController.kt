/*
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  * License, v. 2.0. If a copy of the MPL was not distributed with this
 *  * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.mozilla.fenix.library.history

interface HistoryController {
    fun handleOpen(item: HistoryItem)
    fun handleSelect(item: HistoryItem)
    fun handleDeselect(item: HistoryItem)
    fun handleBackPressed(): Boolean
    fun handleModeSwitched()
    fun handleDeleteAll()
    fun handleDeleteSome(items: Set<HistoryItem>)
}

class DefaultHistoryController(
    private val store: HistoryStore,
    private val openToBrowser: (item: HistoryItem) -> Unit,
    private val displayDeleteAll: () -> Unit,
    private val invalidateOptionsMenu: () -> Unit,
    private val deleteHistoryItems: (Set<HistoryItem>) -> Unit
) : HistoryController {
    override fun handleOpen(item: HistoryItem) {
        openToBrowser(item)
    }

    override fun handleSelect(item: HistoryItem) {
        store.dispatch(HistoryAction.AddItemForRemoval(item))
    }

    override fun handleDeselect(item: HistoryItem) {
        store.dispatch(HistoryAction.RemoveItemForRemoval(item))
    }

    override fun handleBackPressed(): Boolean {
        return if (store.state.mode is HistoryState.Mode.Editing) {
            store.dispatch(HistoryAction.ExitEditMode)
            true
        } else {
            false
        }
    }

    override fun handleModeSwitched() {
        invalidateOptionsMenu.invoke()
    }

    override fun handleDeleteAll() {
        displayDeleteAll.invoke()
    }

    override fun handleDeleteSome(items: Set<HistoryItem>) {
        deleteHistoryItems.invoke(items)
    }
}
