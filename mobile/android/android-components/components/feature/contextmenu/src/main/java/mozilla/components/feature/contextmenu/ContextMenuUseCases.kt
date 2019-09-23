/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.contextmenu

import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.state.content.DownloadState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.HitResult

/**
 * Contains use cases related to the context menu feature.
 *
 * @param sessionManager the application's [SessionManager].
 */
class ContextMenuUseCases(
    sessionManager: SessionManager,
    store: BrowserStore
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
        private val store: BrowserStore
    ) {
        /**
         * Adds a [Download] to the [Session] with the given [tabId].
         *
         * This is a hacky workaround. After we have migrated everything from browser-session to
         * browser-state we should revisits this and find a better solution.
         */
        operator fun invoke(tabId: String, download: DownloadState) {
            store.dispatch(ContentAction.UpdateDownloadAction(
                tabId, download
            ))
        }
    }

    val consumeHitResult = ConsumeHitResultUseCase(sessionManager)
    val injectDownload = InjectDownloadUseCase(store)
}
