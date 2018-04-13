/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.session

/**
 * This class provides access to a centralized registry of all active sessions.
 */
class SessionManager(
    sessionProvider: SessionProvider
) {
    private val sessions: ObservableList<Session>
    private val observers = mutableListOf<Observer>()

    init {
        val (initialSessions, initialSelectedIndex) = sessionProvider.getInitialSessions()

        sessions = ObservableList(
                initialSessions.toMutableList(),
                initialSelectedIndex)
    }

    /**
     * Returns the number of sessions.
     */
    val size: Int
        get() = sessions.size

    /**
     * Gets the currently selected session. Only one session can be selected.
     */
    val selectedSession: Session
        get() = sessions.selected

    /**
     * Registers an observer to get notified about changes regarding sessions (e.g. adding a new
     * session or removing an existing session).
     */
    fun register(observer: Observer) {
        observers.add(observer)
    }

    /**
     * Unregisters an observer.
     */
    fun unregister(observer: Observer) {
        observers.remove(observer)
    }

    /**
     * Marks the given session as selected.
     */
    fun select(session: Session) {
        sessions.select(session)

        observers.forEach { it.onSessionSelected(session) }
    }

    /**
     * Interface to be implemented by classes that want to observe the session manager.
     */
    interface Observer {
        /**
         * The selection has changed and the given session is now the selected session.
         */
        fun onSessionSelected(session: Session)
    }
}
