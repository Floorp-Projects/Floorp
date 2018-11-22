/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import mozilla.components.browser.session.SelectionAwareSessionObserver
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.window.WindowRequest

/**
 * Feature implementation for handling window requests.
 */
class WindowFeature(private val engine: Engine, private val sessionManager: SessionManager) {

    internal val windowObserver = object : SelectionAwareSessionObserver(sessionManager) {
        override fun onOpenWindowRequested(session: Session, windowRequest: WindowRequest): Boolean {
            val newSession = Session(windowRequest.url, session.private)
            val newEngineSession = engine.createSession(session.private)
            windowRequest.prepare(newEngineSession)

            sessionManager.add(newSession, true, newEngineSession, parent = session)
            windowRequest.start()
            return true
        }

        override fun onCloseWindowRequested(session: Session, windowRequest: WindowRequest): Boolean {
            sessionManager.remove(session)
            return true
        }
    }

    /**
     * Starts the feature and a observer to listen for window requests.
     */
    fun start() {
        windowObserver.observeSelected()
    }

    /**
     * Stops the feature and the window request observer.
     */
    fun stop() {
        windowObserver.stop()
    }
}
