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
 * The [Store] for holding the [HistoryState] and applying [HistoryAction]s.
 */
class HistoryStore(initialState: HistoryState) :
    Store<HistoryState, HistoryAction>(initialState, ::historyStateReducer)

/**
 * Actions to dispatch through the `HistoryStore` to modify `HistoryState` through the reducer.
 */
sealed class HistoryAction : Action {
    object ExitEditMode : HistoryAction()
    data class AddItemForRemoval(val item: HistoryItem) : HistoryAction()
    data class RemoveItemForRemoval(val item: HistoryItem) : HistoryAction()
    object EnterDeletionMode : HistoryAction()
    object ExitDeletionMode : HistoryAction()
}

/**
 * The state for the History Screen
 * @property items List of HistoryItem to display
 * @property mode Current Mode of History
 */
data class HistoryState(val items: List<HistoryItem>, val mode: Mode) : State {
    sealed class Mode {
        open val selectedItems = emptySet<HistoryItem>()

        object Normal : Mode()
        object Deleting : Mode()
        data class Editing(override val selectedItems: Set<HistoryItem>) : Mode()
    }
}

/**
 * The HistoryState Reducer.
 */
fun historyStateReducer(state: HistoryState, action: HistoryAction): HistoryState {
    return when (action) {
        is HistoryAction.AddItemForRemoval ->
            state.copy(mode = HistoryState.Mode.Editing(state.mode.selectedItems + action.item))
        is HistoryAction.RemoveItemForRemoval -> {
            val selected = state.mode.selectedItems - action.item
            state.copy(
                mode = if (selected.isEmpty()) HistoryState.Mode.Normal else HistoryState.Mode.Editing(selected)
            )
        }
        is HistoryAction.ExitEditMode -> state.copy(mode = HistoryState.Mode.Normal)
        is HistoryAction.EnterDeletionMode -> state.copy(mode = HistoryState.Mode.Deleting)
        is HistoryAction.ExitDeletionMode -> state.copy(mode = HistoryState.Mode.Normal)
    }
}
