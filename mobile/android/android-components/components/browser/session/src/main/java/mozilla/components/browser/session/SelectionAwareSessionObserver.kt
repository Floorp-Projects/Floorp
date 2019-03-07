/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session

import androidx.annotation.CallSuper

/**
 * This class is a combination of [Session.Observer] and
 * [SessionManager.Observer]. It provides functionality to observe changes to a
 * specified or selected session, and can automatically take care of switching
 * over the observer in case a different session gets selected (see
 * [observeFixed] and [observeSelected]).
 *
 * @property activeSession the currently observed session
 * @property sessionManager the application's session manager
 */
abstract class SelectionAwareSessionObserver(
    private val sessionManager: SessionManager
) : SessionManager.Observer, Session.Observer {

    protected open var activeSession: Session? = null

    /**
     * Starts observing changes to the specified session.
     *
     * @param session the session to observe.
     */
    fun observeFixed(session: Session) {
        activeSession = session
        session.register(this)
    }

    /**
     * Starts observing changes to the selected session (see
     * [SessionManager.selectedSession]). If a different session is selected
     * the observer will automatically be switched over and only notified of
     * changes to the newly selected session.
     */
    fun observeSelected() {
        activeSession = sessionManager.selectedSession
        sessionManager.register(this)
        activeSession?.register(this)
    }

    /**
     * Starts observing changes to the session matching the [sessionId]. If
     * the session does not exist, then observe the selected session.
     *
     * @param sessionId the session ID to observe.
     */
    fun observeIdOrSelected(sessionId: String?) {
        val session = sessionId?.let { sessionManager.findSessionById(sessionId) }
        session?.let { observeFixed(it) } ?: observeSelected()
    }

    /**
     * Stops the observer.
     */
    @CallSuper
    open fun stop() {
        sessionManager.unregister(this)
        activeSession?.unregister(this)
    }

    @CallSuper
    override fun onSessionSelected(session: Session) {
        activeSession?.unregister(this)
        activeSession = session
        session.register(this)
    }
}
