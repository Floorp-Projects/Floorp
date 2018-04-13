/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.mock
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class SessionUseCasesTest {

    @Test
    fun testLoadUrl() {
        val sessionManager = mock(SessionManager::class.java)
        val engine = mock(Engine::class.java)
        val engineSession = mock(EngineSession::class.java)
        val sessionMapping = mock(SessionMapping::class.java)
        `when`(sessionMapping.getOrCreate(engine, sessionManager.selectedSession)).thenReturn(engineSession)

        val useCases = SessionUseCases(sessionManager, engine, sessionMapping)
        useCases.loadUrl.invoke("http://mozilla.org")
        verify(engineSession).loadUrl("http://mozilla.org")
    }
}
