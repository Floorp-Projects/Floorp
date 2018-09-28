/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.storage

import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession

data class SessionWithState(
    val session: Session,
    val selected: Boolean = false,
    val engineSession: EngineSession? = null
)
typealias SessionsSnapshot = List<SessionWithState>

/**
 * Storage component for browser and engine sessions.
 */
interface SessionStorage {

    /**
     * Persists the provided [SessionsSnapshot] for a given [Engine].
     * [SessionsSnapshot] may be obtained using [SessionManager.createSnapshot].
     *
     * @param engine the engine in which context to persist a snapshot.
     * @param snapshot the snapshot of snapshot which are to be persisted.
     * @return true if the snapshot was persisted, otherwise false.
     */
    fun persist(engine: Engine, snapshot: SessionsSnapshot): Boolean

    /**
     * Returns the latest persisted sessions snapshot that may be used to read sessions.
     * Resulting [SessionsSnapshot] may be restored via [SessionManager.restore].
     *
     * @param engine the engine for which to read the snapshot.
     * @return snapshot of sessions to read, or null if they couldn't be read.
     */
    fun read(engine: Engine): SessionsSnapshot?

    /**
     * Starts persisting the state frequently and automatically.
     *
     * @param sessionManager the session manager to persist from.
     */
    fun start(sessionManager: SessionManager)

    /**
     * Stops persisting the state automatically.
     */
    fun stop()
}
