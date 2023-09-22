/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.history

import android.os.Parcelable
import kotlinx.parcelize.Parcelize
import mozilla.components.concept.storage.HistoryMetadata
import mozilla.components.concept.storage.HistoryMetadataKey
import mozilla.components.lib.state.Action
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store
import mozilla.components.support.ktx.kotlin.tryGetHostFromUrl

/**
 * Class representing a history entry.
 */
sealed class History : Parcelable {
    abstract val position: Int
    abstract val title: String
    abstract val visitedAt: Long
    abstract val historyTimeGroup: HistoryItemTimeGroup
    abstract val selected: Boolean

    /**
     * A regular history item.
     *
     * @property position Position of this item in a result list of other [History] items.
     * @property title Title of the history item.
     * @property url URL of the history item.
     * @property visitedAt Timestamp of when this history item was visited.
     * @property historyTimeGroup [HistoryItemTimeGroup] of the history item.
     * @property selected Whether or not the history item is selected.
     * @property isRemote A history item is either opened locally or synced from other devices.
     */
    @Parcelize
    data class Regular(
        override val position: Int,
        override val title: String,
        val url: String,
        override val visitedAt: Long,
        override val historyTimeGroup: HistoryItemTimeGroup,
        override val selected: Boolean = false,
        val isRemote: Boolean = false,
    ) : History()

    /**
     * A history metadata item.
     *
     * @property position Position of this item in a result list of other [History] items.
     * @property title Title of the history metadata item.
     * @property url URL of the history metadata item.
     * @property visitedAt Timestamp of when this history metadata item was visited.
     * @property historyTimeGroup [HistoryItemTimeGroup] of the history item.
     * @property totalViewTime Total time the user viewed the page associated with this record.
     * @property historyMetadataKey The [HistoryMetadataKey] of the new tab in case this tab
     * was opened from history.
     * @property selected Whether or not the history metadata item is selected.
     */
    @Parcelize
    data class Metadata(
        override val position: Int,
        override val title: String,
        val url: String,
        override val visitedAt: Long,
        override val historyTimeGroup: HistoryItemTimeGroup,
        val totalViewTime: Int,
        val historyMetadataKey: HistoryMetadataKey,
        override val selected: Boolean = false,
    ) : History()

    /**
     * A history metadata group.
     *
     * @property position Position of this item in a result list of other [History] items.
     * @property title Title of the history metadata group.
     * @property visitedAt Timestamp of when this history metadata group was visited.
     * @property historyTimeGroup [HistoryItemTimeGroup] of the history item.
     * @property items List of history metadata items associated with the group.
     * @property selected Whether or not the history group is selected.
     */
    @Parcelize
    data class Group(
        override val position: Int,
        override val title: String,
        override val visitedAt: Long,
        override val historyTimeGroup: HistoryItemTimeGroup,
        val items: List<Metadata>,
        override val selected: Boolean = false,
    ) : History()
}

/**
 * Extension function for converting a [HistoryMetadata] into a [History.Metadata].
 */
fun HistoryMetadata.toHistoryMetadata(position: Int): History.Metadata {
    return History.Metadata(
        position = position,
        title = title?.takeIf(String::isNotEmpty)
            ?: key.url.tryGetHostFromUrl(),
        url = key.url,
        visitedAt = createdAt,
        historyTimeGroup = HistoryItemTimeGroup.timeGroupForTimestamp(createdAt),
        totalViewTime = totalViewTime,
        historyMetadataKey = key,
    )
}

/**
 * The [Store] for holding the [HistoryFragmentState] and applying [HistoryFragmentAction]s.
 */
class HistoryFragmentStore(
    initialState: HistoryFragmentState,
    middleware: List<Middleware<HistoryFragmentState, HistoryFragmentAction>> = listOf(),
) :
    Store<HistoryFragmentState, HistoryFragmentAction>(initialState, ::historyStateReducer, middleware)

/**
 * Actions to dispatch through the `HistoryStore` to modify `HistoryState` through the reducer.
 */
sealed class HistoryFragmentAction : Action {
    object ExitEditMode : HistoryFragmentAction()
    data class AddItemForRemoval(val item: History) : HistoryFragmentAction()
    data class RemoveItemForRemoval(val item: History) : HistoryFragmentAction()

    /**
     * A [History] item has been clicked by a user.
     *
     * @property item The history item clicked.
     */
    data class HistoryItemClicked(val item: History) : HistoryFragmentAction()

    /**
     * A [History] item has been long-clicked by a user.
     *
     * @property item The history item long-clicked.
     */
    data class HistoryItemLongClicked(val item: History) : HistoryFragmentAction()

    /**
     * The user has indicated that a time range of history items should be deleted.
     */
    data class DeleteTimeRange(val timeFrame: RemoveTimeFrame?) : HistoryFragmentAction()

    /**
     * The user has indicated that a number of history items should be deleted.
     */
    data class DeleteItems(val items: Set<History>) : HistoryFragmentAction()

    /**
     * The user has clicked to enter the Recently Closed fragment.
     */
    object EnterRecentlyClosed : HistoryFragmentAction()

    /**
     * A back press event has been dispatched.
     */
    object BackPressed : HistoryFragmentAction()

    /**
     * The search menu item has been clicked.
     */
    object SearchClicked : HistoryFragmentAction()

    /**
     * Updates the empty state of [org.mozilla.fenix.library.history.HistoryView].
     */
    data class ChangeEmptyState(val isEmpty: Boolean) : HistoryFragmentAction()

    /**
     * Updates the set of items marked for removal from the [org.mozilla.fenix.components.AppStore]
     * to the [HistoryFragmentStore], to be hidden from the UI.
     */
    data class UpdatePendingDeletionItems(val pendingDeletionItems: Set<PendingDeletionHistory>) :
        HistoryFragmentAction()
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
    val pendingDeletionItems: Set<PendingDeletionHistory>,
    val isEmpty: Boolean,
    val isDeletingItems: Boolean,
) : State {
    sealed class Mode {
        open val selectedItems = emptySet<History>()

        object Normal : Mode()
        object Syncing : Mode()
        data class Editing(override val selectedItems: Set<History>) : Mode()
    }

    companion object {
        val initial = HistoryFragmentState(
            items = listOf(),
            mode = Mode.Normal,
            pendingDeletionItems = emptySet(),
            isEmpty = false,
            isDeletingItems = false,
        )
    }
}

/**
 * The HistoryState Reducer.
 */
private fun historyStateReducer(
    state: HistoryFragmentState,
    action: HistoryFragmentAction,
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
                },
            )
        }
        is HistoryFragmentAction.ExitEditMode -> state.copy(mode = HistoryFragmentState.Mode.Normal)
        is HistoryFragmentAction.EnterDeletionMode -> state.copy(isDeletingItems = true)
        is HistoryFragmentAction.ExitDeletionMode -> state.copy(isDeletingItems = false)
        is HistoryFragmentAction.StartSync -> state.copy(mode = HistoryFragmentState.Mode.Syncing)
        is HistoryFragmentAction.FinishSync -> state.copy(mode = HistoryFragmentState.Mode.Normal)
        is HistoryFragmentAction.ChangeEmptyState -> state.copy(isEmpty = action.isEmpty)
        is HistoryFragmentAction.UpdatePendingDeletionItems -> state.copy(
            pendingDeletionItems = action.pendingDeletionItems,
        )
        is HistoryFragmentAction.HistoryItemClicked -> {
            if (state.mode.selectedItems.isEmpty() || state.mode is HistoryFragmentState.Mode.Syncing) {
                state
            } else {
                if (state.mode.selectedItems.contains(action.item)) {
                    val selected = state.mode.selectedItems - action.item
                    state.copy(
                        mode = if (selected.isEmpty()) {
                            HistoryFragmentState.Mode.Normal
                        } else {
                            HistoryFragmentState.Mode.Editing(selected)
                        },
                    )
                } else {
                    state.copy(
                        mode = HistoryFragmentState.Mode.Editing(
                            state.mode.selectedItems + action.item,
                        ),
                    )
                }
            }
        }
        is HistoryFragmentAction.HistoryItemLongClicked -> {
            if (state.mode == HistoryFragmentState.Mode.Syncing) {
                state
            } else {
                state.copy(
                    mode = HistoryFragmentState.Mode.Editing(
                        state.mode.selectedItems + action.item,
                    ),
                )
            }
        }
        is HistoryFragmentAction.BackPressed -> {
            if (state.mode is HistoryFragmentState.Mode.Editing) {
                state.copy(mode = HistoryFragmentState.Mode.Normal)
            } else {
                state
            }
        }
        // For deletion actions: the item list is handled through storage.
        // Updates from storage are dispatched directly to the view.
        is HistoryFragmentAction.DeleteItems,
        is HistoryFragmentAction.DeleteTimeRange,
        is HistoryFragmentAction.EnterRecentlyClosed,
        is HistoryFragmentAction.SearchClicked,
        -> state
    }
}
