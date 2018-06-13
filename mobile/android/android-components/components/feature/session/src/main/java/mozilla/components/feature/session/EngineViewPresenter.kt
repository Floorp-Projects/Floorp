/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.EngineView

/**
 * Presenter implementation for EngineView.
 */
class EngineViewPresenter(
    private val sessionManager: SessionManager,
    private val engineView: EngineView
) : SessionManager.Observer {

    /**
     * Start presenter and display data in view.
     */
    fun start() {
        renderSession(sessionManager.selectedSession)

        sessionManager.register(this)
    }

    /**
     * Stop presenter from updating view.
     */
    fun stop() {
        sessionManager.unregister(this)
    }

    /**
     * A new session has been selected: Render it on an EngineView.
     */
    override fun onSessionSelected(session: Session) {
        renderSession(session)
    }

    private fun renderSession(session: Session) {
        engineView.render(sessionManager.getOrCreateEngineSession(session))
    }
}
