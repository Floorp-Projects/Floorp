/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.library.downloads

import mozilla.components.lib.state.Action
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store

/**
 * Class representing a history entry
 * @property id Unique id of the download item
 * @property fileName File name of the download item
 * @property filePath Full path of the download item
 * @property size The size in bytes of the download item
 * @property contentType The type of file the download is
 */
data class DownloadItem(val id: Long, val fileName: String?, val filePath: String, val size: String, val contentType: String?)

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
}

/**
 * The state for the Download Screen
 * @property items List of DownloadItem to display
 * @property mode Current Mode of Download
 */
data class DownloadFragmentState(
    val items: List<DownloadItem>,
    val mode: Mode
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
        is DownloadFragmentAction.ExitEditMode -> state.copy(mode = DownloadFragmentState.Mode.Normal)
    }
}
