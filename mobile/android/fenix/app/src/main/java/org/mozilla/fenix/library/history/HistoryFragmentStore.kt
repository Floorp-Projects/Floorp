/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import mozilla.components.lib.state.Action
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store

/**
 * Class representing a history entry
 * @property id Unique id of the history item
 * @property title Title of the history item
 * @property url URL of the history item
 * @property visitedAt Timestamp of when this history item was visited
 */
data class HistoryItem(val id: Int, val title: String, val url: String, val visitedAt: Long)

/**
 * The [Store] for holding the [HistoryFragmentState] and applying [HistoryFragmentAction]s.
 */
class HistoryFragmentStore(initialState: HistoryFragmentState) :
    Store<HistoryFragmentState, HistoryFragmentAction>(initialState, ::historyStateReducer)

/**
 * Actions to dispatch through the `HistoryStore` to modify `HistoryState` through the reducer.
 */
sealed class HistoryFragmentAction : Action {
    object ExitEditMode : HistoryFragmentAction()
    data class AddItemForRemoval(val item: HistoryItem) : HistoryFragmentAction()
    data class RemoveItemForRemoval(val item: HistoryItem) : HistoryFragmentAction()
    data class AddPendingDeletionSet(val itemIds: Set<Long>) : HistoryFragmentAction()
    data class UndoRemovePendingDeletionSet(val itemIds: Set<Long>) : HistoryFragmentAction()
    object EnterDeletionMode : HistoryFragmentAction()
    object ExitDeletionMode : HistoryFragmentAction()
    object StartSync : HistoryFragmentAction()
    object FinishSync : HistoryFragmentAction()
}

/**
 * The state for the History Screen
 * @property items List of HistoryItem to display
 * @property mode Current Mode of History
 */
data class HistoryFragmentState(
    val items: List<HistoryItem>,
    val mode: Mode,
    val pendingDeletionIds: Set<Long>,
    val isDeletingItems: Boolean
) : State {
    sealed class Mode {
        open val selectedItems = emptySet<HistoryItem>()

        object Normal : Mode()
        object Syncing : Mode()
        data class Editing(override val selectedItems: Set<HistoryItem>) : Mode()
    }
}

/**
 * The HistoryState Reducer.
 */
private fun historyStateReducer(
    state: HistoryFragmentState,
    action: HistoryFragmentAction
): HistoryFragmentState {
    return when (action) {
        is HistoryFragmentAction.AddItemForRemoval ->
            state.copy(mode = HistoryFragmentState.Mode.Editing(state.mode.selectedItems + action.item))
        is HistoryFragmentAction.RemoveItemForRemoval -> {
            val selected = state.mode.selectedItems - action.item
            state.copy(
                mode = if (selected.isEmpty()) {
                    HistoryFragmentState.Mode.Normal
                } else {
                    HistoryFragmentState.Mode.Editing(selected)
                }
            )
        }
        is HistoryFragmentAction.ExitEditMode -> state.copy(mode = HistoryFragmentState.Mode.Normal)
        is HistoryFragmentAction.EnterDeletionMode -> state.copy(isDeletingItems = true)
        is HistoryFragmentAction.ExitDeletionMode -> state.copy(isDeletingItems = false)
        is HistoryFragmentAction.StartSync -> state.copy(mode = HistoryFragmentState.Mode.Syncing)
        is HistoryFragmentAction.FinishSync -> state.copy(mode = HistoryFragmentState.Mode.Normal)
        is HistoryFragmentAction.AddPendingDeletionSet ->
            state.copy(
                pendingDeletionIds = state.pendingDeletionIds + action.itemIds
            )
        is HistoryFragmentAction.UndoRemovePendingDeletionSet ->
            state.copy(
                pendingDeletionIds = state.pendingDeletionIds - action.itemIds
            )
    }
}
