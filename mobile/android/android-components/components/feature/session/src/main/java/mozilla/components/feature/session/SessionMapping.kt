/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.browser.session.Session
import java.util.WeakHashMap

/**
 * Helper for mapping Session instances to EngineSession instances.
 */
class SessionMapping {
    private val mapping = WeakHashMap<Session, EngineSession>()

    /**
     * Get the EngineSession corresponding to the given Session. If needed create a new
     * EngineSession instance.
     */
    @Synchronized
    fun getOrCreate(engine: Engine, session: Session): EngineSession {
        return mapping[session] ?: createNewSession(engine, session)
    }

    private fun createNewSession(engine: Engine, session: Session): EngineSession {
        val engineSession = engine.createSession()

        SessionProxy(session, engineSession)
        engineSession.loadUrl(session.url)

        return engineSession
    }
}
