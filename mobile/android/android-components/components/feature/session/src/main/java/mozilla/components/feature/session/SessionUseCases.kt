/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import mozilla.components.browser.session.Session
import mozilla.components.concept.engine.Engine

/**
 * Contains use cases related to the session feature.
 */
class SessionUseCases(
    sessionProvider: SessionProvider,
    engine: Engine
) {

    class LoadUrlUseCase internal constructor(
        private val sessionProvider: SessionProvider,
        private val engine: Engine
    ) {
        /**
         * Loads the provided URL using the currently selected session.
         *
         * @param url url to load.
         */
        fun invoke(url: String) {
            val engineSession = sessionProvider.getOrCreateEngineSession(engine)
            engineSession.loadUrl(url)
        }
    }

    class ReloadUrlUseCase internal constructor(
        private val sessionProvider: SessionProvider
    ) {
        /**
         * Reloads the current URL of the provided session (or the currently
         * selected session if none is provided).
         *
         * @param session the session for which reload should be triggered.
         */
        fun invoke(session: Session = sessionProvider.selectedSession) {
            val engineSession = sessionProvider.getEngineSession(session)
            engineSession?.reload()
        }
    }

    class GoBackUseCase internal constructor(
        private val sessionProvider: SessionProvider,
        private val engine: Engine
    ) {
        /**
         * Navigates back in the history of the currently selected session
         */
        fun invoke() {
            val engineSession = sessionProvider.getOrCreateEngineSession(engine)
            engineSession.goBack()
        }
    }

    class GoForwardUseCase internal constructor(
        private val sessionProvider: SessionProvider,
        private val engine: Engine
    ) {
        /**
         * Navigates forward in the history of the currently selected session
         */
        fun invoke() {
            val engineSession = sessionProvider.getOrCreateEngineSession(engine)
            engineSession.goForward()
        }
    }

    val loadUrl: LoadUrlUseCase by lazy { LoadUrlUseCase(sessionProvider, engine) }
    val reload: ReloadUrlUseCase by lazy { ReloadUrlUseCase(sessionProvider) }
    val goBack: GoBackUseCase by lazy { GoBackUseCase(sessionProvider, engine) }
    val goForward: GoForwardUseCase by lazy { GoForwardUseCase(sessionProvider, engine) }
}
