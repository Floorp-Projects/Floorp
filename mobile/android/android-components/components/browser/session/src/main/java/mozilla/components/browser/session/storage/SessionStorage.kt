/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.storage

import mozilla.components.browser.session.SessionManager

/**
 * Storage component for browser and engine sessions.
 */
interface SessionStorage {

    /**
     * Persists the state of the provided sessions.
     *
     * @param sessionManager the session manager to persist from.
     * @return true if the state was persisted, otherwise false.
     */
    fun persist(sessionManager: SessionManager): Boolean

    /**
     * Restores the session storage state by reading from the latest persisted version.
     *
     * @param sessionManager the session manager to restore into.
     * @return map of all restored sessions, and the currently selected session id.
     */
    fun restore(sessionManager: SessionManager): Boolean

    /**
     * Starts saving the state frequently and automatically.
     *
     * @param sessionManager the session manager to persist from.
     */
    fun start(sessionManager: SessionManager)

    /**
     * Stops saving the state automatically.
     */
    fun stop()
}
