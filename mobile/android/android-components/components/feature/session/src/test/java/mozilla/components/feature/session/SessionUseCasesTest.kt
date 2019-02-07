/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.EngineSession
import mozilla.components.support.test.any
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito.`when`
import org.mockito.Mockito.mock
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

class SessionUseCasesTest {
    private val sessionManager = mock(SessionManager::class.java)
    private val selectedEngineSession = mock(EngineSession::class.java)
    private val selectedSession = mock(Session::class.java)
    private val useCases = SessionUseCases(sessionManager)

    @Before
    fun setup() {
        `when`(sessionManager.selectedSessionOrThrow).thenReturn(selectedSession)
        `when`(sessionManager.selectedSession).thenReturn(selectedSession)
        `when`(sessionManager.getOrCreateEngineSession()).thenReturn(selectedEngineSession)
    }

    @Test
    fun loadUrl() {
        useCases.loadUrl.invoke("http://mozilla.org")
        verify(selectedEngineSession).loadUrl("http://mozilla.org")

        useCases.loadUrl.invoke("http://getpocket.com", selectedSession)
        verify(selectedEngineSession).loadUrl("http://getpocket.com")
    }

    @Test
    fun loadData() {
        useCases.loadData.invoke("<html><body></body></html>", "text/html")
        verify(selectedEngineSession).loadData("<html><body></body></html>", "text/html", "UTF-8")

        useCases.loadData.invoke("Should load in WebView", "text/plain", session = selectedSession)
        verify(selectedEngineSession).loadData("Should load in WebView", "text/plain", "UTF-8")

        useCases.loadData.invoke("ahr0cdovl21vemlsbgeub3jn==", "text/plain", "base64", selectedSession)
        verify(selectedEngineSession).loadData("ahr0cdovl21vemlsbgeub3jn==", "text/plain", "base64")
    }

    @Test
    fun reload() {
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
    fun stopLoading() {
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
    fun goBack() {
        val engineSession = mock(EngineSession::class.java)
        val session = mock(Session::class.java)

        useCases.goBack.invoke(null)
        verify(engineSession, never()).goBack()
        verify(selectedEngineSession, never()).goBack()

        `when`(sessionManager.getOrCreateEngineSession(session)).thenReturn(engineSession)

        useCases.goBack.invoke(session)
        verify(engineSession).goBack()

        `when`(sessionManager.getOrCreateEngineSession(selectedSession)).thenReturn(selectedEngineSession)
        useCases.goBack.invoke()
        verify(selectedEngineSession).goBack()
    }

    @Test
    fun goForward() {
        val engineSession = mock(EngineSession::class.java)
        val session = mock(Session::class.java)

        useCases.goForward.invoke(null)
        verify(engineSession, never()).goForward()
        verify(selectedEngineSession, never()).goForward()

        `when`(sessionManager.getOrCreateEngineSession(session)).thenReturn(engineSession)

        useCases.goForward.invoke(session)
        verify(engineSession).goForward()

        `when`(sessionManager.getOrCreateEngineSession(selectedSession)).thenReturn(selectedEngineSession)
        useCases.goForward.invoke()
        verify(selectedEngineSession).goForward()
    }

    @Test
    fun requestDesktopSite() {
        val engineSession = mock(EngineSession::class.java)
        val session = mock(Session::class.java)
        `when`(sessionManager.getOrCreateEngineSession(session)).thenReturn(engineSession)

        useCases.requestDesktopSite.invoke(true, session)
        verify(engineSession).toggleDesktopMode(true, true)

        `when`(sessionManager.getOrCreateEngineSession(selectedSession)).thenReturn(selectedEngineSession)
        useCases.requestDesktopSite.invoke(true)
        verify(selectedEngineSession).toggleDesktopMode(true, true)
    }

    @Test
    fun exitFullscreen() {
        val engineSession = mock(EngineSession::class.java)
        val session = mock(Session::class.java)
        `when`(sessionManager.getOrCreateEngineSession(session)).thenReturn(engineSession)

        useCases.exitFullscreen.invoke(session)
        verify(engineSession).exitFullScreenMode()

        `when`(sessionManager.getOrCreateEngineSession(selectedSession)).thenReturn(selectedEngineSession)
        useCases.exitFullscreen.invoke()
        verify(selectedEngineSession).exitFullScreenMode()
    }

    @Test
    fun clearData() {
        val engineSession = mock(EngineSession::class.java)
        val session = mock(Session::class.java)
        `when`(sessionManager.getOrCreateEngineSession(session)).thenReturn(engineSession)

        useCases.clearData.invoke(session)
        verify(engineSession).clearData()

        `when`(sessionManager.getOrCreateEngineSession(selectedSession)).thenReturn(selectedEngineSession)
        useCases.clearData.invoke()
        verify(selectedEngineSession).clearData()
    }

    @Test
    fun `LoadUrlUseCase will invoke onNoSession lambda if no selected session exists`() {
        var createdSession: Session? = null
        var sessionCreatedForUrl: String? = null

        `when`(sessionManager.selectedSession).thenReturn(null)
        `when`(sessionManager.getOrCreateEngineSession(any())).thenReturn(mock())

        val loadUseCase = SessionUseCases.DefaultLoadUrlUseCase(sessionManager) { url ->
            sessionCreatedForUrl = url
            Session(url).also { createdSession = it }
        }

        loadUseCase.invoke("https://www.example.com")

        assertEquals("https://www.example.com", sessionCreatedForUrl)
        assertNotNull(createdSession)
        verify(sessionManager).getOrCreateEngineSession(createdSession!!)
    }

    @Test
    fun `LoadDataUseCase will invoke onNoSession lambda if no selected session exists`() {
        var createdSession: Session? = null
        var sessionCreatedForUrl: String? = null

        val engineSession: EngineSession = mock()

        `when`(sessionManager.selectedSession).thenReturn(null)
        `when`(sessionManager.getOrCreateEngineSession(any())).thenReturn(engineSession)

        val loadUseCase = SessionUseCases.LoadDataUseCase(sessionManager) { url ->
            sessionCreatedForUrl = url
            Session(url).also { createdSession = it }
        }

        loadUseCase.invoke("Hello", mimeType = "plain/text", encoding = "UTF-8")

        assertEquals("about:blank", sessionCreatedForUrl)
        assertNotNull(createdSession)
        verify(sessionManager).getOrCreateEngineSession(createdSession!!)
        verify(engineSession).loadData("Hello", mimeType = "plain/text", encoding = "UTF-8")
    }
}
