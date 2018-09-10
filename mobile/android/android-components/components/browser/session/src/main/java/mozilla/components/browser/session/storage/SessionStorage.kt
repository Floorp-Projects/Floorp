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
     * Persists the session state of the provided [SessionManager].
     *
     * @param sessionManager the session manager to persist from.
     * @return true if the state was persisted, otherwise false.
     */
    fun persist(sessionManager: SessionManager): Boolean

    /**
     * Restores the session state by reading from the latest persisted version.
     *
     * @param sessionManager the session manager to restore into.
     * @return true if the state was restored, otherwise false.
     */
    fun restore(sessionManager: SessionManager): Boolean

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
