/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import mozilla.components.browser.session.Session
import mozilla.components.concept.engine.Engine

/**
 * Storage component for browser and engine sessions.
 */
interface SessionStorage {

    /**
     * Persists the current storage state (all active sessions).
     *
     * @param selectedSession the currently selected session, optional.
     * @return true if the state was persisted, otherwise false.
     */
    fun persist(selectedSession: Session? = null): Boolean

    /**
     * Restores the storage state by reading from the latest persisted version.
     *
     * @param engine the engine instance to use. Required to create new engine
     * sessions.
     * @return list of all restored sessions
     */
    fun restore(engine: Engine): List<SessionProxy>

    /**
     * Adds the provided session. The state change is not persisted until
     * [persist] is called.
     *
     * @param session the session to add.
     * @return unique ID of the session.
     */
    fun add(session: SessionProxy): String

    /**
     * Removes the session with the provided ID. The state change is not persisted until
     * [persist] is called.
     *
     * @param id the ID of the session to remove.
     * @return true if the session was found and removed, otherwise false.
     */
    fun remove(id: String): Boolean

    /**
     * Returns the session for the provided ID.
     *
     * @param id the ID of the session.
     * @return the session if found, otherwise null.
     */
    fun get(id: String): SessionProxy?

    /**
     * Returns the selected session.
     *
     * @return the selected session, null if no session is selected.
     */
    fun getSelected(): Session?
}
