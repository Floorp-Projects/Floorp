/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.Engine.BrowsingData
import mozilla.components.concept.engine.EngineSession
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito.`when`
import org.mockito.Mockito.doReturn
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
        verify(engineSession).toggleDesktopMode(true, reload = true)

        `when`(sessionManager.getOrCreateEngineSession(selectedSession)).thenReturn(selectedEngineSession)
        useCases.requestDesktopSite.invoke(true)
        verify(selectedEngineSession).toggleDesktopMode(true, reload = true)
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
        val engine = mock(Engine::class.java)
        `when`(sessionManager.engine).thenReturn(engine)
        `when`(sessionManager.getOrCreateEngineSession(session)).thenReturn(engineSession)

        useCases.clearData.invoke(session)
        verify(engine).clearData()
        verify(engineSession).clearData()

        useCases.clearData.invoke(data = BrowsingData.select(BrowsingData.COOKIES))
        verify(engine).clearData(eq(BrowsingData.select(BrowsingData.COOKIES)), eq(null), any(), any())

        useCases.clearData.invoke(session, data = BrowsingData.select(BrowsingData.COOKIES))
        verify(engineSession).clearData(eq(BrowsingData.select(BrowsingData.COOKIES)), eq(null), any(), any())

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

    @Test
    fun `CrashRecoveryUseCase will invoke recoverFromCrash on engine session and reset flag`() {
        val engineSession = mock(EngineSession::class.java)
        doReturn(true).`when`(engineSession).recoverFromCrash()

        val session = mock(Session::class.java)
        `when`(sessionManager.getOrCreateEngineSession(session)).thenReturn(engineSession)

        assertTrue(useCases.crashRecovery.invoke(session))

        verify(engineSession).recoverFromCrash()
        verify(session).crashed = false
    }

    @Test
    fun `CrashRecoveryUseCase will restore list of crashed sessions`() {
        val engineSession1 = mock(EngineSession::class.java)
        doReturn(true).`when`(engineSession1).recoverFromCrash()

        val engineSession2 = mock(EngineSession::class.java)
        doReturn(true).`when`(engineSession2).recoverFromCrash()

        val session1 = mock(Session::class.java)
        `when`(sessionManager.getOrCreateEngineSession(session1)).thenReturn(engineSession1)

        val session2 = mock(Session::class.java)
        `when`(sessionManager.getOrCreateEngineSession(session2)).thenReturn(engineSession2)

        assertTrue(useCases.crashRecovery.invoke(listOf(session1, session2)))

        verify(engineSession1).recoverFromCrash()
        verify(engineSession2).recoverFromCrash()
        verify(session1).crashed = false
        verify(session2).crashed = false
    }

    @Test
    fun `CrashRecoveryUseCase will restore crashed sessions`() {
        val engineSession1 = mock(EngineSession::class.java)
        doReturn(true).`when`(engineSession1).recoverFromCrash()

        val engineSession2 = mock(EngineSession::class.java)
        doReturn(true).`when`(engineSession2).recoverFromCrash()

        val session1 = mock(Session::class.java)
        `when`(sessionManager.getOrCreateEngineSession(session1)).thenReturn(engineSession1)
        doReturn(true).`when`(session1).crashed

        val session2 = mock(Session::class.java)
        doReturn(false).`when`(session2).crashed
        `when`(sessionManager.getOrCreateEngineSession(session2)).thenReturn(engineSession2)

        doReturn(listOf(session1, session2)).`when`(sessionManager).sessions

        assertTrue(useCases.crashRecovery.invoke())

        verify(engineSession1).recoverFromCrash()
        verify(engineSession2, never()).recoverFromCrash()
        verify(session1).crashed = false
        verify(session2, never()).crashed = false
    }
}
