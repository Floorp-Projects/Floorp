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
    fun testLoadData() {
        useCases.loadData.invoke("<html><body></body></html>", "text/html")
        verify(selectedEngineSession).loadData("<html><body></body></html>", "text/html", "UTF-8")

        useCases.loadData.invoke("Should load in WebView", "text/plain", session = selectedSession)
        verify(selectedEngineSession).loadData("Should load in WebView", "text/plain", "UTF-8")

        useCases.loadData.invoke("ahr0cdovl21vemlsbgeub3jn==", "text/plain", "base64", selectedSession)
        verify(selectedEngineSession).loadData("ahr0cdovl21vemlsbgeub3jn==", "text/plain", "base64")
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
    fun testStopLoading() {
        val engineSession = mock(EngineSession::class.java)
        val session = mock(Session::class.java)
        `when`(sessionManager.getOrCreateEngineSession(session)).thenReturn(engineSession)

        useCases.stopLoading.invoke(session)
        verify(engineSession).stopLoading()

        `when`(sessionManager.getOrCreateEngineSession(selectedSession)).thenReturn(selectedEngineSession)
        useCases.stopLoading.invoke()
        verify(selectedEngineSession).stopLoading()
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

    @Test
    fun testRequestDesktopSite() {
        val engineSession = mock(EngineSession::class.java)
        val session = mock(Session::class.java)
        `when`(sessionManager.getOrCreateEngineSession(session)).thenReturn(engineSession)

        useCases.requestDesktopSite.invoke(true, session)
        verify(engineSession).toggleDesktopMode(true, true)

        `when`(sessionManager.getOrCreateEngineSession(selectedSession)).thenReturn(selectedEngineSession)
        useCases.requestDesktopSite.invoke(true)
        verify(selectedEngineSession).toggleDesktopMode(true, true)
    }
}
