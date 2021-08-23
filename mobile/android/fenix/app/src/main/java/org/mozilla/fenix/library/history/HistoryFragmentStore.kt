/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import android.os.Parcelable
import kotlinx.parcelize.Parcelize
import mozilla.components.lib.state.Action
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store

/**
 * Class representing a history entry.
 */
sealed class History : Parcelable {
    abstract val id: Int
    abstract val title: String
    abstract val visitedAt: Long
    abstract val selected: Boolean

    /**
     * A regular history item.
     *
     * @property id Unique id of the history item.
     * @property title Title of the history item.
     * @property url URL of the history item.
     * @property visitedAt Timestamp of when this history item was visited.
     * @property selected Whether or not the history item is selected.
     */
    @Parcelize data class Regular(
        override val id: Int,
        override val title: String,
        val url: String,
        override val visitedAt: Long,
        override val selected: Boolean = false
    ) : History()

    /**
     * A history metadata item.
     *
     * @property id Unique id of the history metadata item.
     * @property title Title of the history metadata item.
     * @property url URL of the history metadata item.
     * @property visitedAt Timestamp of when this history metadata item was visited.
     * @property totalViewTime Total time the user viewed the page associated with this record.
     * @property selected Whether or not the history metadata item is selected.
     */
    @Parcelize data class Metadata(
        override val id: Int,
        override val title: String,
        val url: String,
        override val visitedAt: Long,
        val totalViewTime: Int,
        override val selected: Boolean = false
    ) : History()

    /**
     * A history metadata group.
     *
     * @property id Unique id of the history metadata group.
     * @property title Title of the history metadata group.
     * @property visitedAt Timestamp of when this history metadata group was visited.
     * @property items List of history metadata items associated with the group.
     * @property selected Whether or not the history group is selected.
     */
    @Parcelize data class Group(
        override val id: Int,
        override val title: String,
        override val visitedAt: Long,
        val items: List<Metadata>,
        override val selected: Boolean = false
    ) : History()
}

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
    data class AddItemForRemoval(val item: History.Regular) : HistoryFragmentAction()
    data class RemoveItemForRemoval(val item: History.Regular) : HistoryFragmentAction()
    data class AddPendingDeletionSet(val itemIds: Set<Long>) : HistoryFragmentAction()
    data class UndoPendingDeletionSet(val itemIds: Set<Long>) : HistoryFragmentAction()
    object EnterDeletionMode : HistoryFragmentAction()
    object ExitDeletionMode : HistoryFragmentAction()
    object StartSync : HistoryFragmentAction()
    object FinishSync : HistoryFragmentAction()
}

/**
 * The state for the History Screen
 * @property items List of History to display
 * @property mode Current Mode of History
 */
data class HistoryFragmentState(
    val items: List<History>,
    val mode: Mode,
    val pendingDeletionIds: Set<Long>,
    val isDeletingItems: Boolean
) : State {
    sealed class Mode {
        open val selectedItems = emptySet<History.Regular>()

        object Normal : Mode()
        object Syncing : Mode()
        data class Editing(override val selectedItems: Set<History.Regular>) : Mode()
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
        is HistoryFragmentAction.UndoPendingDeletionSet ->
            state.copy(
                pendingDeletionIds = state.pendingDeletionIds - action.itemIds
            )
    }
}
