/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
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
    private val selectedEngineSession = mock(EngineSession::class.java)
    private val selectedSession = mock(Session::class.java)
    private val useCases = SessionUseCases(sessionManager)

    @Before
    fun setup() {
        `when`(sessionManager.selectedSessionOrThrow).thenReturn(selectedSession)
        `when`(sessionManager.getOrCreateEngineSession()).thenReturn(selectedEngineSession)
    }

    @Test
    fun testLoadUrl() {
        useCases.loadUrl.invoke("http://mozilla.org")
        verify(selectedEngineSession).loadUrl("http://mozilla.org")

        useCases.loadUrl.invoke("http://getpocket.com", selectedSession)
        verify(selectedEngineSession).loadUrl("http://getpocket.com")
    }

    @Test
    fun testReload() {
        val engineSession = mock(EngineSession::class.java)
        val session = mock(Session::class.java)
        `when`(sessionManager.getOrCreateEngineSession(session)).thenReturn(engineSession)

        useCases.reload.invoke(session)
        verify(engineSession).reload()

        `when`(sessionManager.getOrCreateEngineSession(selectedSession)).thenReturn(selectedEngineSession)
        useCases.reload.invoke()
        verify(selectedEngineSession).reload()
    }

    @Test
    fun testGoBack() {
        useCases.goBack.invoke()
        verify(selectedEngineSession).goBack()
    }

    @Test
    fun testGoForward() {
        useCases.goForward.invoke()
        verify(selectedEngineSession).goForward()
    }
}
