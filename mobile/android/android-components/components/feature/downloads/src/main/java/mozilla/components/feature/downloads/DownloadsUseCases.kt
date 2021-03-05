/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads

import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.DownloadAction
import mozilla.components.browser.state.store.BrowserStore

/**
 * Contains use cases related to the downloads feature.
 *
 * @param store the application's [BrowserStore].
 */
class DownloadsUseCases(
    store: BrowserStore
) {

    /**
     * Use case that cancels the download request from a tab.
     */
    class CancelDownloadRequestUseCase(
        private val store: BrowserStore
    ) {
        /**
         * Cancels the download request the session with the given [tabId].
         */
        operator fun invoke(tabId: String, downloadId: String) {
            store.dispatch(ContentAction.CancelDownloadAction(tabId, downloadId))
        }
    }

    class ConsumeDownloadUseCase(
        private val store: BrowserStore
    ) {
        /**
         * Consumes the download with the given [downloadId] from the session with the given
         * [tabId].
         */
        operator fun invoke(tabId: String, downloadId: String) {
            store.dispatch(ContentAction.ConsumeDownloadAction(
                tabId, downloadId
            ))
        }
    }

    /**
     * Use case that allows to restore downloads from the storage.
     */
    class RestoreDownloadsUseCase(private val store: BrowserStore) {
        /**
         * Restores downloads from the storage.
         */
        operator fun invoke() {
            store.dispatch(DownloadAction.RestoreDownloadsStateAction)
        }
    }

    /**
     * Use case that allows to remove a download.
     */
    class RemoveDownloadUseCase(private val store: BrowserStore) {
        /**
         * Removes the download with the given [downloadId].
         */
        operator fun invoke(downloadId: String) {
            store.dispatch(DownloadAction.RemoveDownloadAction(downloadId))
        }
    }

    /**
     * Use case that allows to remove all downloads.
     */
    class RemoveAllDownloadsUseCase(private val store: BrowserStore) {
        /**
         * Removes all downloads.
         */
        operator fun invoke() {
            store.dispatch(DownloadAction.RemoveAllDownloadsAction)
        }
    }

    val cancelDownloadRequest = CancelDownloadRequestUseCase(store)
    val consumeDownload = ConsumeDownloadUseCase(store)
    val restoreDownloads = RestoreDownloadsUseCase(store)
    val removeDownload = RemoveDownloadUseCase(store)
    val removeAllDownloads = RemoveAllDownloadsUseCase(store)
}
