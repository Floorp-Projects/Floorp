/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.downloads

import mozilla.components.browser.session.Download
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager

/**
 * Contains use cases related to the downloads feature.
 *
 * @param sessionManager the application's [SessionManager].
 */
class DownloadsUseCases(
    sessionManager: SessionManager
) {
    class ConsumeDownloadUseCase(
        private val sessionManager: SessionManager
    ) {
        /**
         * Consumes the [Download] with the given [downloadId] from the [Session] with the given
         * [tabId].
         */
        operator fun invoke(tabId: String, downloadId: String) {
            sessionManager.findSessionById(tabId)?.let { session ->
                session.download.consume {
                    it.id == downloadId
                }
            }
        }
    }

    val consumeDownload = ConsumeDownloadUseCase(sessionManager)
}
