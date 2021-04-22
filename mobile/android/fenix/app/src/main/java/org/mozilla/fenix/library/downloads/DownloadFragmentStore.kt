/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.downloads

import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.lib.state.Action
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store

/**
 * Class representing a downloads entry
 * @property id Unique id of the download item
 * @property url The full url to the content that should be downloaded
 * @property fileName File name of the download item
 * @property filePath Full path of the download item
 * @property size The size in bytes of the download item
 * @property contentType The type of file the download is
 * @property status The status that represents every state that a download can be in
 */
data class DownloadItem(
    val id: String,
    val url: String,
    val fileName: String?,
    val filePath: String,
    val size: String,
    val contentType: String?,
    val status: DownloadState.Status
)

/**
 * The [Store] for holding the [DownloadFragmentState] and applying [DownloadFragmentAction]s.
 */
class DownloadFragmentStore(initialState: DownloadFragmentState) :
    Store<DownloadFragmentState, DownloadFragmentAction>(initialState, ::downloadStateReducer)

/**
 * Actions to dispatch through the `DownloadStore` to modify `DownloadState` through the reducer.
 */

sealed class DownloadFragmentAction : Action {
    object ExitEditMode : DownloadFragmentAction()
    data class AddItemForRemoval(val item: DownloadItem) : DownloadFragmentAction()
    data class RemoveItemForRemoval(val item: DownloadItem) : DownloadFragmentAction()
    data class AddPendingDeletionSet(val itemIds: Set<String>) : DownloadFragmentAction()
    data class UndoPendingDeletionSet(val itemIds: Set<String>) : DownloadFragmentAction()
    object EnterDeletionMode : DownloadFragmentAction()
    object ExitDeletionMode : DownloadFragmentAction()
}

/**
 * The state for the Download Screen
 * @property items List of DownloadItem to display
 * @property mode Current Mode of Download
 */
data class DownloadFragmentState(
    val items: List<DownloadItem>,
    val mode: Mode,
    val pendingDeletionIds: Set<String>,
    val isDeletingItems: Boolean
) : State {
    sealed class Mode {
        open val selectedItems = emptySet<DownloadItem>()

        object Normal : Mode()
        data class Editing(override val selectedItems: Set<DownloadItem>) : DownloadFragmentState.Mode()
    }
}

/**
 * The DownloadState Reducer.
 */
private fun downloadStateReducer(
    state: DownloadFragmentState,
    action: DownloadFragmentAction
): DownloadFragmentState {
    return when (action) {
        is DownloadFragmentAction.AddItemForRemoval ->
            state.copy(mode = DownloadFragmentState.Mode.Editing(state.mode.selectedItems + action.item))
        is DownloadFragmentAction.RemoveItemForRemoval -> {
            val selected = state.mode.selectedItems - action.item
            state.copy(
                mode = if (selected.isEmpty()) {
                    DownloadFragmentState.Mode.Normal
                } else {
                    DownloadFragmentState.Mode.Editing(selected)
                }
            )
        }
        is DownloadFragmentAction.ExitEditMode -> state.copy(mode = DownloadFragmentState.Mode.Normal)
        is DownloadFragmentAction.EnterDeletionMode -> state.copy(isDeletingItems = true)
        is DownloadFragmentAction.ExitDeletionMode -> state.copy(isDeletingItems = false)
        is DownloadFragmentAction.AddPendingDeletionSet ->
            state.copy(
                pendingDeletionIds = state.pendingDeletionIds + action.itemIds
            )
        is DownloadFragmentAction.UndoPendingDeletionSet ->
            state.copy(
                pendingDeletionIds = state.pendingDeletionIds - action.itemIds
            )
    }
}
