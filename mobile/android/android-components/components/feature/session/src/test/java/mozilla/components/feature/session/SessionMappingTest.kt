/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import mozilla.components.browser.session.Session
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import org.junit.Assert.assertEquals
import org.junit.Test
import org.mockito.Mockito.mock
import org.mockito.Mockito.`when`

class SessionMappingTest {

    @Test
    fun testGetOrCreate() {
        val sessionMapping = SessionMapping()
        val session = mock(Session::class.java)
        val engine = mock(Engine::class.java)
        val engineSession1 = mock (EngineSession::class.java)
        val engineSession2 = mock (EngineSession::class.java)

        `when`(engine.createSession()).thenReturn(engineSession1)
        var actualEngineSession = sessionMapping.getOrCreate(engine, session)
        assertEquals(engineSession1, actualEngineSession)

        `when`(engine.createSession()).thenReturn(engineSession2)
        actualEngineSession = sessionMapping.getOrCreate(engine, session)
        // Should still be the original (already created) engine session
        assertEquals(engineSession1, actualEngineSession)
    }
}
