/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import mozilla.components.browser.state.action.DownloadAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.content.DownloadState

internal object DownloadStateReducer {

    /**
     * [DownloadAction] Reducer function for modifying [BrowserState.downloads].
     */
    fun reduce(state: BrowserState, action: DownloadAction): BrowserState {
        return when (action) {
            is DownloadAction.AddDownloadAction -> updateDownloads(state, action.download)
            is DownloadAction.UpdateDownloadAction -> {
                updateDownloads(state, action.download)
            }
            is DownloadAction.DismissDownloadNotificationAction -> {
                val download = state.downloads[action.downloadId]
                if (download != null) {
                    updateDownloads(state, download.copy(notificationId = null))
                } else {
                    state
                }
            }
            is DownloadAction.RemoveDownloadAction -> {
                state.copy(downloads = state.downloads - action.downloadId)
            }
            is DownloadAction.RemoveAllDownloadsAction -> {
                state.copy(downloads = emptyMap())
            }
            is DownloadAction.RestoreDownloadsStateAction -> state
            is DownloadAction.RestoreDownloadStateAction -> updateDownloads(state, action.download)
        }
    }

    private fun updateDownloads(state: BrowserState, download: DownloadState) =
        state.copy(downloads = state.downloads + (download.id to download))
}
