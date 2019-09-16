/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.contextmenu

import mozilla.components.browser.session.Download
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.HitResult
import mozilla.components.support.base.observer.Consumable

/**
 * Contains use cases related to the context menu feature.
 *
 * @param sessionManager the application's [SessionManager].
 */
class ContextMenuUseCases(
    sessionManager: SessionManager
) {
    class ConsumeHitResultUseCase(
        private val sessionManager: SessionManager
    ) {
        /**
         * Consumes the [HitResult] from the [Session] with the given [tabId].
         */
        operator fun invoke(tabId: String) {
            sessionManager.findSessionById(tabId)?.let {
                it.hitResult.consume { true }
            }
        }
    }

    class InjectDownloadUseCase(
        private val sessionManager: SessionManager
    ) {
        /**
         * Adds a [Download] to the [Session] with the given [tabId].
         *
         * This is a hacky workaround. After we have migrated everything from browser-session to
         * browser-state we should revisits this and find a better solution.
         */
        operator fun invoke(tabId: String, download: Download) {
            sessionManager.findSessionById(tabId)?.let {
                it.download = Consumable.from(download)
            }
        }
    }

    val consumeHitResult = ConsumeHitResultUseCase(sessionManager)
    val injectDownload = InjectDownloadUseCase(sessionManager)
}
