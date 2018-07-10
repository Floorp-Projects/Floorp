/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager

/**
 * Contains use cases related to the session feature.
 */
class SessionUseCases(
    sessionManager: SessionManager
) {

    class LoadUrlUseCase internal constructor(
        private val sessionManager: SessionManager
    ) {
        /**
         * Loads the provided URL using the provided session (or the currently
         * selected session if none is provided).
         *
         * @param url url to load.
         * @param session the session for which the URL should be loaded.
         */
        fun invoke(url: String, session: Session = sessionManager.selectedSessionOrThrow) {
            sessionManager.getOrCreateEngineSession(session).loadUrl(url)
        }
    }

    class ReloadUrlUseCase internal constructor(
        private val sessionManager: SessionManager
    ) {
        /**
         * Reloads the current URL of the provided session (or the currently
         * selected session if none is provided).
         *
         * @param session the session for which reload should be triggered.
         */
        fun invoke(session: Session = sessionManager.selectedSessionOrThrow) {
            sessionManager.getOrCreateEngineSession(session).reload()
        }
    }

    class GoBackUseCase internal constructor(
        private val sessionManager: SessionManager
    ) {
        /**
         * Navigates back in the history of the currently selected session
         */
        fun invoke() {
            sessionManager.getOrCreateEngineSession().goBack()
        }
    }

    class GoForwardUseCase internal constructor(
        private val sessionManager: SessionManager
    ) {
        /**
         * Navigates forward in the history of the currently selected session
         */
        fun invoke() {
            sessionManager.getOrCreateEngineSession().goForward()
        }
    }

    val loadUrl: LoadUrlUseCase by lazy { LoadUrlUseCase(sessionManager) }
    val reload: ReloadUrlUseCase by lazy { ReloadUrlUseCase(sessionManager) }
    val goBack: GoBackUseCase by lazy { GoBackUseCase(sessionManager) }
    val goForward: GoForwardUseCase by lazy { GoForwardUseCase(sessionManager) }
}
