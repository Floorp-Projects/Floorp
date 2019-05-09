/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.icons.extension

import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager

/**
 * This helper will dynamically register the given [Session.Observer] on all [Session]s in the [SessionManager]. It
 * automatically registers the observer on added [Session]s and unregisters it if a [Session] gets removed.
 */
internal class AllSessionsObserver internal constructor(
    private val sessionManager: SessionManager,
    private val observer: Session.Observer
) : SessionManager.Observer {
    private val sessions: MutableSet<Session> = mutableSetOf()

    init {
        sessionManager.all.forEach { registerObserver(it) }
        sessionManager.register(this)
    }

    override fun onSessionAdded(session: Session) {
        registerObserver(session)
    }

    override fun onSessionRemoved(session: Session) {
        unregisterObserver(session)
    }

    override fun onSessionsRestored() {
        sessionManager.all.forEach { registerObserver(it) }
    }

    override fun onAllSessionsRemoved() {
        sessions.toList().forEach { unregisterObserver(it) }
    }

    private fun registerObserver(session: Session) {
        if (!sessions.contains(session)) {
            sessions.add(session)
            session.register(observer)
        }
    }

    private fun unregisterObserver(session: Session) {
        if (sessions.contains(session)) {
            session.unregister(observer)
            sessions.remove(session)
        }
    }

    companion object {
        fun register(sessionManager: SessionManager, observer: Session.Observer) =
            AllSessionsObserver(sessionManager, observer)
    }
}
