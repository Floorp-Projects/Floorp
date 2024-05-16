/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.downloads

import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.lib.state.Action
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store
import org.mozilla.fenix.library.downloads.DownloadFragmentState.Mode

/**
 * Class representing a downloads entry
 * @property id Unique id of the download item
 * @property url The full url to the content that should be downloaded
 * @property fileName File name of the download item
 * @property filePath Full path of the download item
 * @property formattedSize The formatted size of the download item
 * @property contentType The type of file the download is
 * @property status The status that represents every state that a download can be in
 */
data class DownloadItem(
    val id: String,
    val url: String,
    val fileName: String?,
    val filePath: String,
    val formattedSize: String,
    val contentType: String?,
    val status: DownloadState.Status,
)

/**
 * The [Store] for holding the [DownloadFragmentState] and applying [DownloadFragmentAction]s.
 */
class DownloadFragmentStore(
    initialState: DownloadFragmentState,
    middleware: List<Middleware<DownloadFragmentState, DownloadFragmentAction>> = emptyList(),
) : Store<DownloadFragmentState, DownloadFragmentAction>(
    initialState = initialState,
    reducer = ::downloadStateReducer,
    middleware = middleware,
) {

    init {
        dispatch(DownloadFragmentAction.Init)
    }
}

/**
 * Actions to dispatch through the `DownloadStore` to modify `DownloadState` through the reducer.
 */

sealed interface DownloadFragmentAction : Action {
    /**
     * [DownloadFragmentAction] to initialize the state.
     */
    data object Init : DownloadFragmentAction

    /**
     * [DownloadFragmentAction] to exit edit mode.
     */
    data object ExitEditMode : DownloadFragmentAction

    /**
     * [DownloadFragmentAction] add an item to the removal list.
     */
    data class AddItemForRemoval(val item: DownloadItem) : DownloadFragmentAction

    /**
     * [DownloadFragmentAction] to add all items to the removal list.
     */
    data object AddAllItemsForRemoval : DownloadFragmentAction

    /**
     * [DownloadFragmentAction] to remove an item from the removal list.
     */
    data class RemoveItemForRemoval(val item: DownloadItem) : DownloadFragmentAction

    /**
     * [DownloadFragmentAction] to add a set of [DownloadItem] IDs to the pending deletion set.
     */
    data class AddPendingDeletionSet(val itemIds: Set<String>) : DownloadFragmentAction

    /**
     * [DownloadFragmentAction] to undo a set of [DownloadItem] IDs from the pending deletion set.
     */
    data class UndoPendingDeletionSet(val itemIds: Set<String>) : DownloadFragmentAction

    /**
     * [DownloadFragmentAction] to enter deletion mode.
     */
    data object EnterDeletionMode : DownloadFragmentAction

    /**
     * [DownloadFragmentAction] to exit deletion mode.
     */
    data object ExitDeletionMode : DownloadFragmentAction

    /**
     * [DownloadFragmentAction] to update the list of [DownloadItem]s.
     */
    data class UpdateDownloadItems(val items: List<DownloadItem>) : DownloadFragmentAction
}

/**
 * The state of the Download screen.
 *
 * @property items List of [DownloadItem] to display.
 * @property mode Current [Mode] of the Download screen.
 * @property pendingDeletionIds Set of [DownloadItem] IDs that are waiting to be deleted.
 * @property isDeletingItems Whether or not download items are being deleted.
 */
data class DownloadFragmentState(
    val items: List<DownloadItem>,
    val mode: Mode,
    val pendingDeletionIds: Set<String>,
    val isDeletingItems: Boolean,
) : State {

    /**
     * The list of items to display, excluding any items that are pending deletion.
     */
    val itemsToDisplay = items.filter { it.id !in pendingDeletionIds }

    /**
     * Whether or not the state is in an empty state.
     */
    val isEmptyState: Boolean = itemsToDisplay.isEmpty()

    /**
     * Whether or not the state is in normal mode.
     */
    val isNormalMode: Boolean = mode is Mode.Normal

    companion object {
        val INITIAL = DownloadFragmentState(
            items = emptyList(),
            mode = Mode.Normal,
            pendingDeletionIds = emptySet(),
            isDeletingItems = false,
        )
    }

    sealed class Mode {
        open val selectedItems = emptySet<DownloadItem>()

        data object Normal : Mode()
        data class Editing(override val selectedItems: Set<DownloadItem>) : Mode()
    }
}

/**
 * The DownloadState Reducer.
 */
private fun downloadStateReducer(
    state: DownloadFragmentState,
    action: DownloadFragmentAction,
): DownloadFragmentState {
    return when (action) {
        is DownloadFragmentAction.AddItemForRemoval ->
            state.copy(mode = Mode.Editing(state.mode.selectedItems + action.item))

        is DownloadFragmentAction.AddAllItemsForRemoval -> {
            state.copy(mode = Mode.Editing(state.itemsToDisplay.toSet()))
        }

        is DownloadFragmentAction.RemoveItemForRemoval -> {
            val selected = state.mode.selectedItems - action.item
            state.copy(
                mode = if (selected.isEmpty()) {
                    Mode.Normal
                } else {
                    Mode.Editing(selected)
                },
            )
        }

        is DownloadFragmentAction.ExitEditMode -> state.copy(mode = Mode.Normal)
        is DownloadFragmentAction.EnterDeletionMode -> state.copy(isDeletingItems = true)
        is DownloadFragmentAction.ExitDeletionMode -> state.copy(isDeletingItems = false)
        is DownloadFragmentAction.AddPendingDeletionSet ->
            state.copy(
                pendingDeletionIds = state.pendingDeletionIds + action.itemIds,
            )

        is DownloadFragmentAction.UndoPendingDeletionSet ->
            state.copy(
                pendingDeletionIds = state.pendingDeletionIds - action.itemIds,
            )

        is DownloadFragmentAction.UpdateDownloadItems -> state.copy(
            items = action.items.filter { it.id !in state.pendingDeletionIds },
        )

        DownloadFragmentAction.Init -> state
    }
}
