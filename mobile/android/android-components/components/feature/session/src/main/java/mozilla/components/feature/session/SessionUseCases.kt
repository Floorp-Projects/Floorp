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

    class LoadDataUseCase internal constructor(
        private val sessionManager: SessionManager
    ) {
        /**
         * Loads the provided data based on the mime type using the provided session (or the
         * currently selected session if none is provided).
         */
        fun invoke(
            data: String,
            mimeType: String,
            encoding: String = "UTF-8",
            session: Session = sessionManager.selectedSessionOrThrow
        ) {
            sessionManager.getOrCreateEngineSession(session).loadData(data, mimeType, encoding)
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

    class StopLoadingUseCase internal constructor(
        private val sessionManager: SessionManager
    ) {
        /**
         * Stops the current URL of the provided session from loading.
         *
         * @param session the session for which loading should be stopped.
         */
        fun invoke(session: Session = sessionManager.selectedSessionOrThrow) {
            sessionManager.getOrCreateEngineSession(session).stopLoading()
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

    class RequestDesktopSiteUseCase internal constructor(
        private val sessionManager: SessionManager
    ) {
        /**
         * Request the desktop version of the current session and reloads the page.
         */
        fun invoke(enable: Boolean, session: Session = sessionManager.selectedSessionOrThrow) {
            sessionManager.getOrCreateEngineSession(session).toggleDesktopMode(enable, true)
        }
    }

    class ClearDataUseCase internal constructor(
        private val sessionManager: SessionManager
    ) {
        /**
         * Clears all user data sources available.
         */
        fun invoke(session: Session = sessionManager.selectedSessionOrThrow) {
            sessionManager.getOrCreateEngineSession(session).clearData()
        }
    }

    val loadUrl: LoadUrlUseCase by lazy { LoadUrlUseCase(sessionManager) }
    val loadData: LoadDataUseCase by lazy { LoadDataUseCase(sessionManager) }
    val reload: ReloadUrlUseCase by lazy { ReloadUrlUseCase(sessionManager) }
    val stopLoading: StopLoadingUseCase by lazy { StopLoadingUseCase(sessionManager) }
    val goBack: GoBackUseCase by lazy { GoBackUseCase(sessionManager) }
    val goForward: GoForwardUseCase by lazy { GoForwardUseCase(sessionManager) }
    val requestDesktopSite: RequestDesktopSiteUseCase by lazy { RequestDesktopSiteUseCase(sessionManager) }
    val clearData: ClearDataUseCase by lazy { ClearDataUseCase(sessionManager) }
}
