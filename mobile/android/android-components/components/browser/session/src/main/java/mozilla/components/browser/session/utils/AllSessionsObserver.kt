/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.utils

import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager

/**
 * This class is a combination of [Session.Observer] and [SessionManager.Observer]. It observers all [Session] instances
 * that get added to the [SessionManager] and automatically unsubscribes from [Session] instances that get removed.
 *
 * @property sessionManager the application's session manager
 */
class AllSessionsObserver(
    private val sessionManager: SessionManager,
    private val sessionObserver: Session.Observer
) {
    private val observer = Observer(this)
    private val registeredSessions: MutableSet<Session> = mutableSetOf()

    fun start() {
        registerToAllSessions()
        sessionManager.register(observer)
    }

    fun stop() {
        sessionManager.unregister(observer)
        unregisterAllSessions()
    }

    internal fun registerToAllSessions() {
        sessionManager.all.forEach { session -> registerSession(session) }
    }

    internal fun registerSession(session: Session) {
        if (session !in registeredSessions) {
            session.register(sessionObserver)
            registeredSessions.add(session)
        }
    }

    internal fun unregisterSession(session: Session) {
        registeredSessions.remove(session)
        session.unregister(sessionObserver)
    }

    internal fun unregisterAllSessions() {
        registeredSessions.toList().forEach { session ->
            unregisterSession(session)
        }
    }
}

private class Observer(
    val parent: AllSessionsObserver
) : SessionManager.Observer {
    override fun onSessionAdded(session: Session) = parent.registerSession(session)

    override fun onSessionRemoved(session: Session) = parent.unregisterSession(session)

    override fun onAllSessionsRemoved() = parent.unregisterAllSessions()

    override fun onSessionsRestored() = parent.registerToAllSessions()
}
