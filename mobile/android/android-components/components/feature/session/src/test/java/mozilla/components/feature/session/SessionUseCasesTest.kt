/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.mock
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class SessionUseCasesTest {
    private val sessionManager = mock(SessionManager::class.java)
    private val engine = mock(Engine::class.java)
    private val engineSession = mock(EngineSession::class.java)
    private val sessionProvider = mock(SessionProvider::class.java)
    private val useCases = SessionUseCases(sessionProvider, engine)

    @Before
    fun setup() {
        `when`(sessionProvider.sessionManager).thenReturn(sessionManager)
        `when`(sessionProvider.getOrCreateEngineSession(engine)).thenReturn(engineSession)
    }

    @Test
    fun testLoadUrl() {
        useCases.loadUrl.invoke("http://mozilla.org")
        verify(engineSession).loadUrl("http://mozilla.org")
    }

    @Test
    fun testGoBack() {
        useCases.goBack.invoke()
        verify(engineSession).goBack()
    }

    @Test
    fun testGoForward() {
        useCases.goForward.invoke()
        verify(engineSession).goForward()
    }
}
