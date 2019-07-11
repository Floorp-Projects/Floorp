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
    data class Change(val list: List<HistoryItem>) : HistoryAction()
    data class EnterEditMode(val item: HistoryItem) : HistoryAction()
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
        object Normal : Mode()
        data class Editing(val selectedItems: List<HistoryItem>) : Mode()
        object Deleting : Mode()
    }
}

/**
 * The HistoryState Reducer.
 */
fun historyStateReducer(state: HistoryState, action: HistoryAction): HistoryState {
    return when (action) {
        is HistoryAction.Change -> state.copy(mode = HistoryState.Mode.Normal, items = action.list)
        is HistoryAction.EnterEditMode -> state.copy(
            mode = HistoryState.Mode.Editing(listOf(action.item))
        )
        is HistoryAction.AddItemForRemoval -> {
            val mode = state.mode
            if (mode is HistoryState.Mode.Editing) {
                val items = mode.selectedItems + listOf(action.item)
                state.copy(mode = HistoryState.Mode.Editing(items))
            } else {
                state
            }
        }
        is HistoryAction.RemoveItemForRemoval -> {
            var mode = state.mode

            if (mode is HistoryState.Mode.Editing) {
                val items = mode.selectedItems.filter { it.id != action.item.id }
                mode = if (items.isEmpty()) HistoryState.Mode.Normal else HistoryState.Mode.Editing(
                    items
                )

                state.copy(mode = mode)
            } else {
                state
            }
        }
        is HistoryAction.ExitEditMode -> state.copy(mode = HistoryState.Mode.Normal)
        is HistoryAction.EnterDeletionMode -> state.copy(mode = HistoryState.Mode.Deleting)
        is HistoryAction.ExitDeletionMode -> state.copy(mode = HistoryState.Mode.Normal)
    }
}
