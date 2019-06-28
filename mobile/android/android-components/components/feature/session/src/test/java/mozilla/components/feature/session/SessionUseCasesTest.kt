/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.Engine.BrowsingData
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.verify

class SessionUseCasesTest {

    private val sessionManager = mock<SessionManager>()
    private val selectedEngineSession = mock<EngineSession>()
    private val selectedSession = mock<Session>()
    private val useCases = SessionUseCases(sessionManager)

    @Before
    fun setup() {
        whenever(sessionManager.selectedSessionOrThrow).thenReturn(selectedSession)
        whenever(sessionManager.selectedSession).thenReturn(selectedSession)
        whenever(sessionManager.getOrCreateEngineSession()).thenReturn(selectedEngineSession)
    }

    @Test
    fun loadUrl() {
        useCases.loadUrl("http://mozilla.org")
        verify(selectedEngineSession).loadUrl("http://mozilla.org")

        useCases.loadUrl("http://www.mozilla.org", LoadUrlFlags.select(LoadUrlFlags.EXTERNAL))
        verify(selectedEngineSession).loadUrl("http://www.mozilla.org", LoadUrlFlags.select(LoadUrlFlags.EXTERNAL))

        useCases.loadUrl("http://getpocket.com", selectedSession)
        verify(selectedEngineSession).loadUrl("http://getpocket.com")

        useCases.loadUrl.invoke("http://www.getpocket.com", selectedSession,
                LoadUrlFlags.select(LoadUrlFlags.BYPASS_PROXY))
        verify(selectedEngineSession).loadUrl("http://www.getpocket.com",
                LoadUrlFlags.select(LoadUrlFlags.BYPASS_PROXY))
    }

    @Test
    fun loadData() {
        useCases.loadData("<html><body></body></html>", "text/html")
        verify(selectedEngineSession).loadData("<html><body></body></html>", "text/html", "UTF-8")

        useCases.loadData("Should load in WebView", "text/plain", session = selectedSession)
        verify(selectedEngineSession).loadData("Should load in WebView", "text/plain", "UTF-8")

        useCases.loadData("ahr0cdovl21vemlsbgeub3jn==", "text/plain", "base64", selectedSession)
        verify(selectedEngineSession).loadData("ahr0cdovl21vemlsbgeub3jn==", "text/plain", "base64")
    }

    @Test
    fun reload() {
        val engineSession = mock<EngineSession>()
        val session = mock<Session>()
        whenever(sessionManager.getOrCreateEngineSession(session)).thenReturn(engineSession)

        useCases.reload(session)
        verify(engineSession).reload()

        whenever(sessionManager.getOrCreateEngineSession(selectedSession)).thenReturn(selectedEngineSession)
        useCases.reload()
        verify(selectedEngineSession).reload()
    }

    @Test
    fun stopLoading() {
        val engineSession = mock<EngineSession>()
        val session = mock<Session>()
        whenever(sessionManager.getOrCreateEngineSession(session)).thenReturn(engineSession)

        useCases.stopLoading(session)
        verify(engineSession).stopLoading()

        whenever(sessionManager.getOrCreateEngineSession(selectedSession)).thenReturn(selectedEngineSession)
        useCases.stopLoading()
        verify(selectedEngineSession).stopLoading()
    }

    @Test
    fun goBack() {
        val engineSession = mock<EngineSession>()
        val session = mock<Session>()

        useCases.goBack(null)
        verify(engineSession, never()).goBack()
        verify(selectedEngineSession, never()).goBack()

        whenever(sessionManager.getOrCreateEngineSession(session)).thenReturn(engineSession)

        useCases.goBack(session)
        verify(engineSession).goBack()

        whenever(sessionManager.getOrCreateEngineSession(selectedSession)).thenReturn(selectedEngineSession)
        useCases.goBack()
        verify(selectedEngineSession).goBack()
    }

    @Test
    fun goForward() {
        val engineSession = mock<EngineSession>()
        val session = mock<Session>()

        useCases.goForward(null)
        verify(engineSession, never()).goForward()
        verify(selectedEngineSession, never()).goForward()

        whenever(sessionManager.getOrCreateEngineSession(session)).thenReturn(engineSession)

        useCases.goForward(session)
        verify(engineSession).goForward()

        whenever(sessionManager.getOrCreateEngineSession(selectedSession)).thenReturn(selectedEngineSession)
        useCases.goForward()
        verify(selectedEngineSession).goForward()
    }

    @Test
    fun requestDesktopSite() {
        val engineSession = mock<EngineSession>()
        val session = mock<Session>()
        whenever(sessionManager.getOrCreateEngineSession(session)).thenReturn(engineSession)

        useCases.requestDesktopSite(true, session)
        verify(engineSession).toggleDesktopMode(true, reload = true)

        whenever(sessionManager.getOrCreateEngineSession(selectedSession)).thenReturn(selectedEngineSession)
        useCases.requestDesktopSite(true)
        verify(selectedEngineSession).toggleDesktopMode(true, reload = true)
    }

    @Test
    fun exitFullscreen() {
        val engineSession = mock<EngineSession>()
        val session = mock<Session>()
        whenever(sessionManager.getOrCreateEngineSession(session)).thenReturn(engineSession)

        useCases.exitFullscreen(session)
        verify(engineSession).exitFullScreenMode()

        whenever(sessionManager.getOrCreateEngineSession(selectedSession)).thenReturn(selectedEngineSession)
        useCases.exitFullscreen()
        verify(selectedEngineSession).exitFullScreenMode()
    }

    @Test
    fun clearData() {
        val engineSession = mock<EngineSession>()
        val session = mock<Session>()
        val engine = mock<Engine>()
        whenever(sessionManager.engine).thenReturn(engine)
        whenever(sessionManager.getOrCreateEngineSession(session)).thenReturn(engineSession)

        useCases.clearData(session)
        verify(engine).clearData()
        verify(engineSession).clearData()

        useCases.clearData(data = BrowsingData.select(BrowsingData.COOKIES))
        verify(engine).clearData(eq(BrowsingData.select(BrowsingData.COOKIES)), eq(null), any(), any())

        useCases.clearData(session, data = BrowsingData.select(BrowsingData.COOKIES))
        verify(engineSession).clearData(eq(BrowsingData.select(BrowsingData.COOKIES)), eq(null), any(), any())

        whenever(sessionManager.getOrCreateEngineSession(selectedSession)).thenReturn(selectedEngineSession)
        useCases.clearData()
        verify(selectedEngineSession).clearData()
    }

    @Test
    fun `LoadUrlUseCase will invoke onNoSession lambda if no selected session exists`() {
        var createdSession: Session? = null
        var sessionCreatedForUrl: String? = null

        whenever(sessionManager.selectedSession).thenReturn(null)
        whenever(sessionManager.getOrCreateEngineSession(any())).thenReturn(mock())

        val loadUseCase = SessionUseCases.DefaultLoadUrlUseCase(sessionManager) { url ->
            sessionCreatedForUrl = url
            Session(url).also { createdSession = it }
        }

        loadUseCase("https://www.example.com")

        assertEquals("https://www.example.com", sessionCreatedForUrl)
        assertNotNull(createdSession)
        verify(sessionManager).getOrCreateEngineSession(createdSession!!)
    }

    @Test
    fun `LoadDataUseCase will invoke onNoSession lambda if no selected session exists`() {
        var createdSession: Session? = null
        var sessionCreatedForUrl: String? = null

        val engineSession: EngineSession = mock()

        whenever(sessionManager.selectedSession).thenReturn(null)
        whenever(sessionManager.getOrCreateEngineSession(any())).thenReturn(engineSession)

        val loadUseCase = SessionUseCases.LoadDataUseCase(sessionManager) { url ->
            sessionCreatedForUrl = url
            Session(url).also { createdSession = it }
        }

        loadUseCase("Hello", mimeType = "plain/text", encoding = "UTF-8")

        assertEquals("about:blank", sessionCreatedForUrl)
        assertNotNull(createdSession)
        verify(sessionManager).getOrCreateEngineSession(createdSession!!)
        verify(engineSession).loadData("Hello", mimeType = "plain/text", encoding = "UTF-8")
    }

    @Test
    fun `CrashRecoveryUseCase will invoke recoverFromCrash on engine session and reset flag`() {
        val engineSession = mock<EngineSession>()
        doReturn(true).`when`(engineSession).recoverFromCrash()

        val session = mock<Session>()
        whenever(sessionManager.getOrCreateEngineSession(session)).thenReturn(engineSession)

        assertTrue(useCases.crashRecovery.invoke(session))

        verify(engineSession).recoverFromCrash()
        verify(session).crashed = false
    }

    @Test
    fun `CrashRecoveryUseCase will restore list of crashed sessions`() {
        val engineSession1 = mock<EngineSession>()
        doReturn(true).`when`(engineSession1).recoverFromCrash()

        val engineSession2 = mock<EngineSession>()
        doReturn(true).`when`(engineSession2).recoverFromCrash()

        val session1 = mock<Session>()
        whenever(sessionManager.getOrCreateEngineSession(session1)).thenReturn(engineSession1)

        val session2 = mock<Session>()
        whenever(sessionManager.getOrCreateEngineSession(session2)).thenReturn(engineSession2)

        assertTrue(useCases.crashRecovery.invoke(listOf(session1, session2)))

        verify(engineSession1).recoverFromCrash()
        verify(engineSession2).recoverFromCrash()
        verify(session1).crashed = false
        verify(session2).crashed = false
    }

    @Test
    fun `CrashRecoveryUseCase will restore crashed sessions`() {
        val engineSession1 = mock<EngineSession>()
        doReturn(true).`when`(engineSession1).recoverFromCrash()

        val engineSession2 = mock<EngineSession>()
        doReturn(true).`when`(engineSession2).recoverFromCrash()

        val session1 = mock<Session>()
        whenever(sessionManager.getOrCreateEngineSession(session1)).thenReturn(engineSession1)
        doReturn(true).`when`(session1).crashed

        val session2 = mock<Session>()
        doReturn(false).`when`(session2).crashed
        whenever(sessionManager.getOrCreateEngineSession(session2)).thenReturn(engineSession2)

        doReturn(listOf(session1, session2)).`when`(sessionManager).sessions

        assertTrue(useCases.crashRecovery.invoke())

        verify(engineSession1).recoverFromCrash()
        verify(engineSession2, never()).recoverFromCrash()
        verify(session1).crashed = false
        verify(session2, never()).crashed = false
    }
}
