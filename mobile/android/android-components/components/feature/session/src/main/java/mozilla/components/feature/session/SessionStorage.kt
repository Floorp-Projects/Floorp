/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import mozilla.components.browser.session.Session
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession

/**
 * Storage component for browser and engine sessions.
 */
interface SessionStorage {

    /**
     * Persists the state of the provided sessions.
     *
     * @param sessions a map from browser session to engine session
     * @param selectedSession an optional ID of the currently selected session.
     * @return true if the state was persisted, otherwise false.
     */
    fun persist(sessions: Map<Session, EngineSession>, selectedSession: String = ""): Boolean

    /**
     * Restores the session storage state by reading from the latest persisted version.
     *
     * @param engine the engine instance to use when creating new engine sessions.
     * @return map of all restored sessions, and the currently selected session id.
     */
    fun restore(engine: Engine): Pair<Map<Session, EngineSession>, String>
}
