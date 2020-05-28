/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import mozilla.components.browser.state.action.DownloadAction
import mozilla.components.browser.state.state.BrowserState

internal object DownloadStateReducer {

    /**
     * [DownloadAction] Reducer function for modifying [BrowserState.queuedDownloads].
     */
    fun reduce(state: BrowserState, action: DownloadAction): BrowserState {
        return when (action) {
            is DownloadAction.QueueDownloadAction -> {
                state.copy(queuedDownloads = state.queuedDownloads + (action.download.id to action.download))
            }
            is DownloadAction.RemoveQueuedDownloadAction -> {
                state.copy(queuedDownloads = state.queuedDownloads - action.downloadId)
            }
            is DownloadAction.RemoveAllQueuedDownloadsAction -> {
                state.copy(queuedDownloads = emptyMap())
            }
        }
    }
}
