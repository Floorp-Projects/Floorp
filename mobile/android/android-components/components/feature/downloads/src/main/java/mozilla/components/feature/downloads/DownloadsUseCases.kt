/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads

import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.store.BrowserStore

/**
 * Contains use cases related to the downloads feature.
 *
 * @param store the application's [BrowserStore].
 */
class DownloadsUseCases(
    store: BrowserStore
) {
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

    val consumeDownload = ConsumeDownloadUseCase(store)
}
