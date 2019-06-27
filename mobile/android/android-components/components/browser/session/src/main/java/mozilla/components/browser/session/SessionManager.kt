/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session

import mozilla.components.browser.session.ext.syncDispatch
import mozilla.components.browser.session.ext.toCustomTabSessionState
import mozilla.components.browser.session.ext.toTabSessionState
import mozilla.components.browser.state.action.CustomTabListAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSessionState
import mozilla.components.support.base.observer.Observable

/**
 * This class provides access to a centralized registry of all active sessions.
 */
@Suppress("TooManyFunctions")
class SessionManager(
    val engine: Engine,
    val defaultSession: (() -> Session)? = null,
    private val store: BrowserStore? = null,
    private val delegate: LegacySessionManager = LegacySessionManager(engine, defaultSession)
) : Observable<SessionManager.Observer> by delegate {
    /**
     * Returns the number of session including CustomTab sessions.
     */
    val size: Int
        get() = delegate.size

    /**
     * Produces a snapshot of this manager's state, suitable for restoring via [SessionManager.restore].
     * Only regular sessions are included in the snapshot. Private and Custom Tab sessions are omitted.
     *
     * @return [Snapshot] of the current session state.
     */
    fun createSnapshot(): Snapshot = delegate.createSnapshot()

    /**
     * Produces a [Snapshot.Item] of a single [Session], suitable for restoring via [SessionManager.restore].
     */
    fun createSessionSnapshot(session: Session): Snapshot.Item = delegate.createSessionSnapshot(session)

    /**
     * Gets the currently selected session if there is one.
     *
     * Only one session can be selected at a given time.
     */
    val selectedSession
        get() = delegate.selectedSession

    /**
     * Gets the currently selected session or throws an IllegalStateException if no session is
     * selected.
     *
     * It's application specific whether a session manager can have no selected session (= no sessions)
     * or not. In applications that always have at least one session dealing with the nullable
     * <code>selectedSession</code> property can be cumbersome. In those situations, where you always
     * expect a session to exist, you can use <code>selectedSessionOrThrow</code> to avoid dealing
     * with null values.
     *
     * Only one session can be selected at a given time.
     */
    val selectedSessionOrThrow: Session
        get() = delegate.selectedSessionOrThrow

    /**
     * Returns a list of active sessions and filters out sessions used for CustomTabs.
     */
    val sessions: List<Session>
        get() = delegate.sessions

    /**
     * Returns a list of all active sessions (including CustomTab sessions).
     */
    val all: List<Session>
        get() = delegate.all

    /**
     * Adds the provided session.
     */
    fun add(
        session: Session,
        selected: Boolean = false,
        engineSession: EngineSession? = null,
        parent: Session? = null
    ) {
        // Add store to Session so that it can dispatch actions whenever it changes.
        session.store = store

        delegate.add(session, selected, engineSession, parent)

        if (session.isCustomTabSession()) {
            store?.syncDispatch(
                CustomTabListAction.AddCustomTabAction(
                    session.toCustomTabSessionState()
                )
            )
        } else {
            store?.syncDispatch(
                TabListAction.AddTabAction(
                    session.toTabSessionState(),
                    select = selected
                )
            )
        }
    }

    /**
     * Restores sessions from the provided [Snapshot].
     *
     * Notification behaviour is as follows:
     * - onSessionAdded notifications will not fire,
     * - onSessionSelected notification will fire exactly once if the snapshot isn't empty (See [updateSelection]
     *   parameter),
     * - once snapshot has been restored, and appropriate session has been selected, onSessionsRestored
     *   notification will fire.
     *
     * @param snapshot A [Snapshot] which may be produced by [createSnapshot].
     * @param updateSelection Whether the selected session should be updated from the restored snapshot.
     */
    fun restore(snapshot: Snapshot, updateSelection: Boolean = true) {
        delegate.restore(snapshot, updateSelection)

        val tabs = snapshot.sessions.mapNotNull { item ->
            if (!item.session.isCustomTabSession()) {
                item.session.toTabSessionState()
            } else {
                // A restored snapshot should not contain any custom tab so we should be able to safely ignore
                // them here.
                null
            }
        }

        val selectedTabId = if (updateSelection && snapshot.selectedSessionIndex != NO_SELECTION) {
            val index = snapshot.selectedSessionIndex
            if (index in 0..snapshot.sessions.lastIndex) {
                snapshot.sessions[index].session.id
            } else {
                null
            }
        } else {
            null
        }

        store?.syncDispatch(TabListAction.RestoreAction(tabs, selectedTabId))
    }

    /**
     * Gets the linked engine session for the provided session (if it exists).
     */
    fun getEngineSession(session: Session = selectedSessionOrThrow) = delegate.getEngineSession(session)

    /**
     * Gets the linked engine session for the provided session and creates it if needed.
     */
    fun getOrCreateEngineSession(session: Session = selectedSessionOrThrow): EngineSession {
        return delegate.getOrCreateEngineSession(session)
    }

    /**
     * Removes the provided session. If no session is provided then the selected session is removed.
     */
    fun remove(
        session: Session = selectedSessionOrThrow,
        selectParentIfExists: Boolean = false
    ) {
        delegate.remove(session, selectParentIfExists)

        if (session.isCustomTabSession()) {
            store?.syncDispatch(
                CustomTabListAction.RemoveCustomTabAction(session.id)
            )
        } else {
            store?.syncDispatch(
                TabListAction.RemoveTabAction(session.id)
            )
        }
    }

    /**
     * Removes all sessions but CustomTab sessions.
     */
    fun removeSessions() {
        delegate.removeSessions()
    }

    /**
     * Removes all sessions including CustomTab sessions.
     */
    fun removeAll() {
        delegate.removeAll()
    }

    /**
     * Marks the given session as selected.
     */
    fun select(session: Session) {
        delegate.select(session)

        store?.syncDispatch(
            TabListAction.SelectTabAction(session.id)
        )
    }

    /**
     * Finds and returns the session with the given id. Returns null if no matching session could be
     * found.
     */
    fun findSessionById(id: String) = delegate.findSessionById(id)

    /**
     * Informs this [SessionManager] that the OS is in low memory condition so it
     * can reduce its allocated objects.
     */
    fun onLowMemory() = delegate.onLowMemory()

    companion object {
        const val NO_SELECTION = -1
    }

    data class Snapshot(
        val sessions: List<Item>,
        val selectedSessionIndex: Int
    ) {
        fun isEmpty() = sessions.isEmpty()

        data class Item(
            val session: Session,
            val engineSession: EngineSession? = null,
            val engineSessionState: EngineSessionState? = null
        )

        companion object {
            fun empty() = Snapshot(emptyList(), NO_SELECTION)
            fun singleItem(item: Item) = Snapshot(listOf(item), NO_SELECTION)
        }
    }

    /**
     * Interface to be implemented by classes that want to observe the session manager.
     */
    interface Observer {
        /**
         * The selection has changed and the given session is now the selected session.
         */
        fun onSessionSelected(session: Session) = Unit

        /**
         * The given session has been added.
         */
        fun onSessionAdded(session: Session) = Unit

        /**
         * Sessions have been restored via a snapshot. This callback is invoked at the end of the
         * call to <code>read</code>, after every session in the snapshot was added, and
         * appropriate session was selected.
         */
        fun onSessionsRestored() = Unit

        /**
         * The given session has been removed.
         */
        fun onSessionRemoved(session: Session) = Unit

        /**
         * All sessions have been removed. Note that this will callback will be invoked whenever
         * <code>removeAll()</code> or <code>removeSessions</code> have been called on the
         * <code>SessionManager</code>. This callback will NOT be invoked when just the last
         * session has been removed by calling <code>remove()</code> on the <code>SessionManager</code>.
         */
        fun onAllSessionsRemoved() = Unit
    }
}

/**
 * Tries to find a session with the provided session ID and runs the block if found.
 *
 * @return True if the session was found and run successfully.
 */
fun SessionManager.runWithSession(
    sessionId: String?,
    block: SessionManager.(Session) -> Boolean
): Boolean {
    sessionId?.let {
        findSessionById(sessionId)?.let { session ->
            return block(session)
        }
    }
    return false
}

/**
 * Tries to find a session with the provided session ID or uses the selected session and runs the block if found.
 *
 * @return True if the session was found and run successfully.
 */
fun SessionManager.runWithSessionIdOrSelected(
    sessionId: String?,
    block: SessionManager.(Session) -> Unit
): Boolean {
    sessionId?.let {
        findSessionById(sessionId)?.let { session ->
            block(session)
            return true
        }
    }
    selectedSession?.let {
        block(it)
        return true
    }
    return false
}
