/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import kotlinx.coroutines.runBlocking
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.state.action.CrashAction
import mozilla.components.browser.state.action.CustomTabListAction
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.createCustomTab
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.Engine.BrowsingData
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

class SessionUseCasesTest {

    private val sessionManager: SessionManager = mock()
    private val selectedSessionId = "testSession"
    private val selectedSession: Session = mock()
    private val store: BrowserStore = mock()
    private val useCases = SessionUseCases(store, sessionManager)

    @Before
    fun setup() {
        whenever(selectedSession.id).thenReturn(selectedSessionId)
        whenever(sessionManager.selectedSessionOrThrow).thenReturn(selectedSession)
        whenever(sessionManager.selectedSession).thenReturn(selectedSession)
    }

    @Test
    fun loadUrl() {
        useCases.loadUrl("http://mozilla.org")
        verify(store).dispatch(EngineAction.LoadUrlAction(selectedSessionId, "http://mozilla.org"))

        useCases.loadUrl("http://www.mozilla.org", LoadUrlFlags.select(LoadUrlFlags.EXTERNAL))
        verify(store).dispatch(EngineAction.LoadUrlAction(
            selectedSessionId,
            "http://www.mozilla.org",
            LoadUrlFlags.select(LoadUrlFlags.EXTERNAL)
        ))

        useCases.loadUrl("http://getpocket.com", selectedSession)
        verify(store).dispatch(EngineAction.LoadUrlAction(selectedSessionId, "http://getpocket.com"))

        useCases.loadUrl.invoke(
            "http://getpocket.com",
            selectedSession,
            LoadUrlFlags.select(LoadUrlFlags.BYPASS_PROXY)
        )
        verify(store).dispatch(EngineAction.LoadUrlAction(
            selectedSessionId,
            "http://getpocket.com",
            LoadUrlFlags.select(LoadUrlFlags.BYPASS_PROXY)
        ))
    }

    @Test
    fun loadData() {
        useCases.loadData("<html><body></body></html>", "text/html")
        verify(store).dispatch(EngineAction.LoadDataAction(
            selectedSessionId,
            "<html><body></body></html>",
            "text/html",
            "UTF-8")
        )
        useCases.loadData("Should load in WebView", "text/plain", session = selectedSession)
        verify(store).dispatch(EngineAction.LoadDataAction(
            selectedSessionId,
            "Should load in WebView",
            "text/plain",
            "UTF-8")
        )

        useCases.loadData("Should also load in WebView", "text/plain", "base64", selectedSession)
        verify(store).dispatch(EngineAction.LoadDataAction(
            selectedSessionId,
            "Should also load in WebView",
            "text/plain",
            "base64")
        )
    }

    @Test
    fun reload() {
        useCases.reload()
        verify(store).dispatch(EngineAction.ReloadAction(selectedSessionId))

        whenever(sessionManager.findSessionById("testSession")).thenReturn(selectedSession)
        useCases.reload(selectedSessionId, LoadUrlFlags.select(LoadUrlFlags.EXTERNAL))
        verify(store).dispatch(EngineAction.ReloadAction(selectedSessionId, LoadUrlFlags.select(LoadUrlFlags.EXTERNAL)))
    }

    @Test
    fun reloadBypassCache() {
        val flags = LoadUrlFlags.select(LoadUrlFlags.BYPASS_CACHE)
        useCases.reload(selectedSession, flags = flags)
        verify(store).dispatch(EngineAction.ReloadAction(selectedSessionId, flags))
    }

    @Test
    fun stopLoading() = runBlocking {
        val store = spy(BrowserStore())
        val useCases = SessionUseCases(store, sessionManager)
        val engineSession: EngineSession = mock()
        store.dispatch(TabListAction.AddTabAction(createTab("https://wwww.mozilla.org", id = selectedSessionId))).joinBlocking()
        store.dispatch(EngineAction.LinkEngineSessionAction(selectedSessionId, engineSession)).joinBlocking()

        useCases.stopLoading()
        verify(engineSession).stopLoading()

        useCases.stopLoading(selectedSession)
        verify(engineSession, times(2)).stopLoading()
    }

    @Test
    fun goBack() {
        useCases.goBack(null)
        verify(store, never()).dispatch(EngineAction.GoBackAction(selectedSessionId))

        useCases.goBack(selectedSession)
        verify(store).dispatch(EngineAction.GoBackAction(selectedSessionId))

        useCases.goBack()
        verify(store, times(2)).dispatch(EngineAction.GoBackAction(selectedSessionId))
    }

    @Test
    fun goForward() {
        useCases.goForward(null)
        verify(store, never()).dispatch(EngineAction.GoForwardAction(selectedSessionId))

        useCases.goForward(selectedSession)
        verify(store).dispatch(EngineAction.GoForwardAction(selectedSessionId))

        useCases.goForward()
        verify(store, times(2)).dispatch(EngineAction.GoForwardAction(selectedSessionId))
    }

    @Test
    fun goToHistoryIndex() {
        useCases.goToHistoryIndex(session = null, index = 0)
        verify(store, never()).dispatch(EngineAction.GoToHistoryIndexAction(selectedSessionId, 0))

        useCases.goToHistoryIndex(session = selectedSession, index = 0)
        verify(store).dispatch(EngineAction.GoToHistoryIndexAction(selectedSessionId, 0))

        useCases.goToHistoryIndex(index = 0)
        verify(store, times(2)).dispatch(EngineAction.GoToHistoryIndexAction(selectedSessionId, 0))
    }

    @Test
    fun requestDesktopSite() {
        useCases.requestDesktopSite(true, selectedSession)
        verify(store).dispatch(EngineAction.ToggleDesktopModeAction(selectedSessionId, true))

        useCases.requestDesktopSite(false)
        verify(store).dispatch(EngineAction.ToggleDesktopModeAction(selectedSessionId, false))
    }

    @Test
    fun exitFullscreen() {
        useCases.exitFullscreen(selectedSession)
        verify(store).dispatch(EngineAction.ExitFullScreenModeAction(selectedSessionId))

        useCases.exitFullscreen()
        verify(store, times(2)).dispatch(EngineAction.ExitFullScreenModeAction(selectedSessionId))
    }

    @Test
    fun clearData() {
        val engine: Engine = mock()
        whenever(sessionManager.engine).thenReturn(engine)

        useCases.clearData(selectedSession)
        verify(engine).clearData()
        verify(store).dispatch(EngineAction.ClearDataAction(selectedSessionId, BrowsingData.all()))

        useCases.clearData(data = BrowsingData.select(BrowsingData.COOKIES))
        verify(store).dispatch(EngineAction.ClearDataAction(selectedSessionId,
            BrowsingData.select(BrowsingData.COOKIES))
        )

        useCases.clearData(selectedSession, data = BrowsingData.select(BrowsingData.IMAGE_CACHE))
        verify(store).dispatch(EngineAction.ClearDataAction(selectedSessionId,
            BrowsingData.select(BrowsingData.IMAGE_CACHE))
        )

        useCases.clearData()
        verify(store, times(2)).dispatch(EngineAction.ClearDataAction(selectedSessionId, BrowsingData.all()))
    }

    @Test
    fun `LoadUrlUseCase will invoke onNoSession lambda if no selected session exists`() {
        var createdSession: Session? = null
        var sessionCreatedForUrl: String? = null
        whenever(sessionManager.selectedSession).thenReturn(null)

        val loadUseCase = SessionUseCases.DefaultLoadUrlUseCase(store, sessionManager) { url ->
            sessionCreatedForUrl = url
            Session(url).also { createdSession = it }
        }

        loadUseCase("https://www.example.com")

        assertEquals("https://www.example.com", sessionCreatedForUrl)
        assertNotNull(createdSession)

        val actionCaptor = argumentCaptor<EngineAction.LoadUrlAction>()
        verify(store).dispatch(actionCaptor.capture())
        assertEquals(createdSession!!.id, actionCaptor.value.sessionId)
        assertEquals(sessionCreatedForUrl, actionCaptor.value.url)
    }

    @Test
    fun `LoadDataUseCase will invoke onNoSession lambda if no selected session exists`() {
        var createdSession: Session? = null
        var sessionCreatedForUrl: String? = null
        whenever(sessionManager.selectedSession).thenReturn(null)

        val loadUseCase = SessionUseCases.LoadDataUseCase(store, sessionManager) { url ->
            sessionCreatedForUrl = url
            Session(url).also { createdSession = it }
        }

        loadUseCase("Hello", mimeType = "text/plain", encoding = "UTF-8")

        assertEquals("about:blank", sessionCreatedForUrl)
        assertNotNull(createdSession)

        val actionCaptor = argumentCaptor<EngineAction.LoadDataAction>()
        verify(store).dispatch(actionCaptor.capture())
        assertEquals(createdSession!!.id, actionCaptor.value.sessionId)
        assertEquals("Hello", actionCaptor.value.data)
        assertEquals("text/plain", actionCaptor.value.mimeType)
        assertEquals("UTF-8", actionCaptor.value.encoding)
    }

    @Test
    fun `CrashRecoveryUseCase will restore specified session`() {
        useCases.crashRecovery.invoke(listOf(selectedSessionId))
        verify(store).dispatch(CrashAction.RestoreCrashedSessionAction(selectedSessionId))
    }

    @Test
    fun `CrashRecoveryUseCase will restore list of crashed sessions`() {
        val store = spy(BrowserStore())
        val useCases = SessionUseCases(store, sessionManager)

        store.dispatch(TabListAction.AddTabAction(createTab("https://wwww.mozilla.org", id = "tab1"))).joinBlocking()
        store.dispatch(CustomTabListAction.AddCustomTabAction(createCustomTab("https://wwww.mozilla.org", id = "customTab1"))).joinBlocking()
        store.dispatch(CrashAction.SessionCrashedAction("tab1")).joinBlocking()
        store.dispatch(CrashAction.SessionCrashedAction("customTab1")).joinBlocking()

        useCases.crashRecovery.invoke()
        verify(store).dispatch(CrashAction.RestoreCrashedSessionAction("tab1"))
        verify(store).dispatch(CrashAction.RestoreCrashedSessionAction("customTab1"))
    }
}
