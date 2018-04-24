/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine

/**
 * Contains use cases related to the session feature.
 */
class SessionUseCases(
    sessionManager: SessionManager,
    engine: Engine,
    sessionMapping: SessionMapping
) {

    class LoadUrlUseCase internal constructor(
        private val sessionManager: SessionManager,
        private val engine: Engine,
        private val sessionMapping: SessionMapping
    ) {
        /**
         * Loads the provided URL using the currently selected session.
         *
         * @param url url to load.
         */
        fun invoke(url: String) {
            val engineSession = sessionMapping.getOrCreate(engine, sessionManager.selectedSession)
            engineSession.loadUrl(url)
        }
    }

    class GoBackUseCase internal constructor(
        private val sessionManager: SessionManager,
        private val engine: Engine,
        private val sessionMapping: SessionMapping
    ) {
        /**
         * Navigates back in the history of the currently selected session
         */
        fun invoke() {
            val engineSession = sessionMapping.getOrCreate(engine, sessionManager.selectedSession)
            engineSession.goBack()
        }
    }

    class GoForwardUseCase internal constructor(
        private val sessionManager: SessionManager,
        private val engine: Engine,
        private val sessionMapping: SessionMapping
    ) {
        /**
         * Navigates forward in the history of the currently selected session
         */
        fun invoke() {
            val engineSession = sessionMapping.getOrCreate(engine, sessionManager.selectedSession)
            engineSession.goForward()
        }
    }

    val loadUrl: LoadUrlUseCase by lazy { LoadUrlUseCase(sessionManager, engine, sessionMapping) }
    val goBack: GoBackUseCase by lazy { GoBackUseCase(sessionManager, engine, sessionMapping) }
    val goForward: GoForwardUseCase by lazy { GoForwardUseCase(sessionManager, engine, sessionMapping) }
}
