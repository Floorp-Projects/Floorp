/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs

import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.test.TestCoroutineDispatcher
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.session.engine.EngineMiddleware
import mozilla.components.browser.session.storage.SessionStorage
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.state.SessionState.Source
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.state.recover.RecoverableTab
import mozilla.components.browser.state.state.recover.toRecoverableTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags
import mozilla.components.concept.engine.EngineSessionState
import mozilla.components.support.test.any
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.mockito.ArgumentMatchers.anyBoolean
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

const val DAY_IN_MS = 24 * 60 * 60 * 1000L

class TabsUseCasesTest {
    private val dispatcher: TestCoroutineDispatcher = TestCoroutineDispatcher()

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule(dispatcher)

    @Test
    fun `SelectTabUseCase - session will be selected in session manager`() {
        val sessionManager: SessionManager = mock()
        val useCases = TabsUseCases(BrowserStore(), sessionManager)

        val session = Session("A")
        doReturn(session).`when`(sessionManager).findSessionById(session.id)

        useCases.selectTab(session.id)

        verify(sessionManager).select(session)
    }

    @Test
    fun `RemoveTabUseCase - session will be removed in session manager`() {
        val sessionManager: SessionManager = mock()
        val useCases = TabsUseCases(BrowserStore(), sessionManager)

        val session = Session("A")
        doReturn(session).`when`(sessionManager).findSessionById(session.id)

        useCases.removeTab(session.id)

        verify(sessionManager).remove(session, false)
    }

    @Test
    fun `RemoveTabUseCase - session can be removed by ID`() {
        val sessionManager: SessionManager = mock()
        val session = Session(id = "test", initialUrl = "http://mozilla.org")
        whenever(sessionManager.findSessionById(session.id)).thenReturn(session)

        val useCases = TabsUseCases(BrowserStore(), sessionManager)
        useCases.removeTab(session.id)
        verify(sessionManager).remove(session, false)
    }

    @Test
    fun `RemoveTabUseCase - remove by ID and select parent if it exists`() {
        val sessionManager: SessionManager = mock()
        val session = Session(id = "test", initialUrl = "http://mozilla.org")
        whenever(sessionManager.findSessionById(session.id)).thenReturn(session)

        val useCases = TabsUseCases(BrowserStore(), sessionManager)
        useCases.removeTab(session.id, selectParentIfExists = true)
        verify(sessionManager).remove(session, true)
    }

    @Test
    fun `RemoveTabsUseCase - list of sessions can be removed`() {
        val sessionManager = spy(SessionManager(mock()))
        val useCases = TabsUseCases(BrowserStore(), sessionManager)

        val session = Session(id = "test", initialUrl = "http://mozilla.org")
        val session2 = Session(id = "test2", initialUrl = "http://pocket.com")
        val session3 = Session(id = "test3", initialUrl = "http://firefox.com")

        sessionManager.add(listOf(session, session2, session3))
        assertEquals(3, sessionManager.size)

        useCases.removeTabs.invoke(listOf(session.id, session2.id))

        verify(sessionManager).removeListOfSessions(listOf(session.id, session2.id))

        assertEquals(1, sessionManager.size)
    }

    @Test
    fun `AddNewTabUseCase - session will be added to session manager`() {
        val sessionManager = spy(SessionManager(mock()))
        val store: BrowserStore = mock()
        val useCases = TabsUseCases(store, sessionManager)

        assertEquals(0, sessionManager.size)

        useCases.addTab("https://www.mozilla.org")

        assertEquals(1, sessionManager.size)
        assertEquals("https://www.mozilla.org", sessionManager.selectedSessionOrThrow.url)
        assertEquals(Source.NEW_TAB, sessionManager.selectedSessionOrThrow.source)
        assertFalse(sessionManager.selectedSessionOrThrow.private)

        val actionCaptor = argumentCaptor<EngineAction.LoadUrlAction>()
        verify(store).dispatch(actionCaptor.capture())
        assertEquals("https://www.mozilla.org", actionCaptor.value.url)
    }

    @Test
    fun `AddNewPrivateTabUseCase - private session will be added to session manager`() {
        val sessionManager = spy(SessionManager(mock()))
        val store: BrowserStore = mock()
        val useCases = TabsUseCases(store, sessionManager)

        assertEquals(0, sessionManager.size)

        useCases.addPrivateTab("https://www.mozilla.org")

        assertEquals(1, sessionManager.size)
        assertEquals("https://www.mozilla.org", sessionManager.selectedSessionOrThrow.url)
        assertEquals(Source.NEW_TAB, sessionManager.selectedSessionOrThrow.source)
        assertTrue(sessionManager.selectedSessionOrThrow.private)

        val actionCaptor = argumentCaptor<EngineAction.LoadUrlAction>()
        verify(store).dispatch(actionCaptor.capture())
        assertEquals("https://www.mozilla.org", actionCaptor.value.url)
    }

    @Test
    fun `AddNewTabUseCase will not load URL if flag is set to false`() {
        val sessionManager = spy(SessionManager(mock()))
        val store: BrowserStore = mock()
        val useCases = TabsUseCases(store, sessionManager)

        useCases.addTab("https://www.mozilla.org", startLoading = false)

        val actionCaptor = argumentCaptor<EngineAction.LoadUrlAction>()
        verify(store, never()).dispatch(actionCaptor.capture())
    }

    @Test
    fun `AddNewTabUseCase will load URL if flag is set to true`() {
        val sessionManager: SessionManager = mock()
        val store: BrowserStore = mock()
        val useCases = TabsUseCases(store, sessionManager)

        useCases.addTab("https://www.mozilla.org", startLoading = true)
        val actionCaptor = argumentCaptor<EngineAction.LoadUrlAction>()
        verify(store).dispatch(actionCaptor.capture())
        assertEquals("https://www.mozilla.org", actionCaptor.value.url)
    }

    @Test
    fun `AddNewTabUseCase forwards load flags to engine`() {
        val engineSession: EngineSession = mock()
        val engine: Engine = mock()

        var sessionLookup: ((String) -> Session?)? = null

        whenever(engine.createSession(anyBoolean(), any())).thenReturn(engineSession)
        val store = BrowserStore(
            middleware = EngineMiddleware.create(
                engine = engine,
                sessionLookup = { sessionLookup?.invoke(it) }
            )
        )
        val sessionManager = SessionManager(engine, store)
        sessionLookup = { sessionManager.findSessionById(it) }
        val tabsUseCases = TabsUseCases(store, sessionManager)

        tabsUseCases.addTab.invoke("https://www.mozilla.org", flags = LoadUrlFlags.external(), startLoading = true)

        // Wait for CreateEngineSessionAction and middleware
        store.waitUntilIdle()
        dispatcher.advanceUntilIdle()

        // Wait for LinkEngineSessionAction and middleware
        store.waitUntilIdle()
        dispatcher.advanceUntilIdle()

        assertEquals(1, store.state.tabs.size)
        assertEquals("https://www.mozilla.org", store.state.tabs[0].content.url)
        verify(engineSession, times(1)).loadUrl("https://www.mozilla.org", null, LoadUrlFlags.external(), null)
    }

    @Test
    fun `AddNewTabUseCase uses provided engine session`() {
        val store = BrowserStore()
        val sessionManager = spy(SessionManager(mock(), store))
        val engineSession: EngineSession = mock()

        val useCases = TabsUseCases(store, sessionManager)
        useCases.addTab.invoke("https://www.mozilla.org", engineSession = engineSession, startLoading = true)
        assertEquals(1, store.state.tabs.size)
        assertEquals(engineSession, store.state.tabs.first().engineState.engineSession)
    }

    @Test
    fun `AddNewTabUseCase uses provided contextId`() {
        val sessionManager = spy(SessionManager(mock()))
        val useCases = TabsUseCases(BrowserStore(), sessionManager)

        assertEquals(0, sessionManager.size)

        useCases.addTab("https://www.mozilla.org", contextId = "1")

        assertEquals(1, sessionManager.size)
        assertEquals("https://www.mozilla.org", sessionManager.selectedSessionOrThrow.url)
        assertEquals("1", sessionManager.selectedSessionOrThrow.contextId)
        assertEquals(Source.NEW_TAB, sessionManager.selectedSessionOrThrow.source)
        assertFalse(sessionManager.selectedSessionOrThrow.private)
    }

    @Test
    fun `AddNewPrivateTabUseCase will not load URL if flag is set to false`() {
        val sessionManager = spy(SessionManager(mock()))
        val store: BrowserStore = mock()
        val useCases = TabsUseCases(store, sessionManager)

        useCases.addPrivateTab("https://www.mozilla.org", startLoading = false)

        val actionCaptor = argumentCaptor<EngineAction.LoadUrlAction>()
        verify(store, never()).dispatch(actionCaptor.capture())
    }

    @Test
    fun `AddNewPrivateTabUseCase will load URL if flag is set to true`() {
        val sessionManager = spy(SessionManager(mock()))
        val store: BrowserStore = mock()
        val useCases = TabsUseCases(store, sessionManager)

        useCases.addPrivateTab("https://www.mozilla.org", startLoading = true)

        val actionCaptor = argumentCaptor<EngineAction.LoadUrlAction>()
        verify(store).dispatch(actionCaptor.capture())
        assertEquals("https://www.mozilla.org", actionCaptor.value.url)
    }

    @Test
    fun `AddNewPrivateTabUseCase forwards load flags to engine`() {
        val sessionManager: SessionManager = mock()
        val store: BrowserStore = mock()
        val useCases = TabsUseCases(store, sessionManager)

        useCases.addPrivateTab.invoke("https://www.mozilla.org", LoadUrlFlags.select(LoadUrlFlags.EXTERNAL))
        val actionCaptor = argumentCaptor<EngineAction.LoadUrlAction>()
        verify(store).dispatch(actionCaptor.capture())
        assertEquals("https://www.mozilla.org", actionCaptor.value.url)
        assertEquals(LoadUrlFlags.select(LoadUrlFlags.EXTERNAL), actionCaptor.value.flags)
    }

    @Test
    fun `AddNewPrivateTabUseCase uses provided engine session`() {
        val store = BrowserStore()
        val sessionManager = spy(SessionManager(mock(), store))
        val engineSession: EngineSession = mock()

        val useCases = TabsUseCases(store, sessionManager)
        useCases.addPrivateTab.invoke("https://www.mozilla.org", engineSession = engineSession, startLoading = true)
        assertEquals(1, store.state.tabs.size)
        assertEquals(engineSession, store.state.tabs.first().engineState.engineSession)
    }

    @Test
    fun `RemoveAllTabsUseCase will remove all sessions`() {
        val sessionManager = spy(SessionManager(mock()))
        val useCases = TabsUseCases(BrowserStore(), sessionManager)

        useCases.addPrivateTab("https://www.mozilla.org")
        useCases.addTab("https://www.mozilla.org")
        assertEquals(2, sessionManager.size)

        useCases.removeAllTabs()
        assertEquals(0, sessionManager.size)
        verify(sessionManager).removeSessions()
    }

    @Test
    fun `RemoveNormalTabsUseCase and RemovePrivateTabsUseCase will remove sessions for particular type of tabs private or normal`() {
        val sessionManager = spy(SessionManager(mock()))
        val useCases = TabsUseCases(BrowserStore(), sessionManager)

        val session1 = Session("https://www.mozilla.org")
        session1.customTabConfig = mock()
        sessionManager.add(session1)
        useCases.addPrivateTab("https://www.mozilla.org")
        useCases.addTab("https://www.mozilla.org")
        assertEquals(3, sessionManager.size)

        useCases.removeNormalTabs.invoke()
        assertEquals(2, sessionManager.all.size)

        useCases.addPrivateTab("https://www.mozilla.org")
        useCases.addTab("https://www.mozilla.org")
        useCases.addTab("https://www.mozilla.org")
        assertEquals(5, sessionManager.size)

        useCases.removePrivateTabs.invoke()
        assertEquals(3, sessionManager.size)

        useCases.removeNormalTabs.invoke()
        assertEquals(1, sessionManager.size)

        assertTrue(sessionManager.all[0].isCustomTabSession())
    }

    @Test
    fun `RestoreUseCase - filters based on tab timeout`() = runBlocking {
        val sessionManager: SessionManager = mock()
        val useCases = TabsUseCases(BrowserStore(), sessionManager)

        val now = System.currentTimeMillis()
        val tabs = listOf(
            createTab("https://mozilla.org", lastAccess = now).toRecoverableTab(),
            createTab("https://firefox.com", lastAccess = now - 2 * DAY_IN_MS).toRecoverableTab(),
            createTab("https://getpocket.com", lastAccess = now - 3 * DAY_IN_MS).toRecoverableTab()
        )

        val sessionStorage: SessionStorage = mock()
        useCases.restore(sessionStorage, tabTimeoutInMs = DAY_IN_MS)

        val predicateCaptor = argumentCaptor<(RecoverableTab) -> Boolean>()
        verify(sessionStorage).restore(predicateCaptor.capture())

        // Only the first tab should be restored, all others "timed out."
        val restoredTabs = tabs.filter(predicateCaptor.value)
        assertEquals(1, restoredTabs.size)
        assertEquals(tabs.first(), restoredTabs.first())
    }

    @Test
    fun `Restore single tab, update selection - SessionManager and BrowserStore are in sync`() {
        val store = BrowserStore()
        val sessionManager = SessionManager(store = store, engine = mock())

        sessionManager.add(Session("https://www.mozilla.org", id = "mozilla"))

        assertEquals(1, sessionManager.sessions.size)
        assertEquals(1, store.state.tabs.size)
        assertEquals("mozilla", sessionManager.selectedSessionOrThrow.id)
        assertEquals("mozilla", store.state.selectedTabId)

        val closedTab = RecoverableTab(
            id = "wikipedia",
            url = "https://www.wikipedia.org"
        )

        val useCases = TabsUseCases(store, sessionManager)
        useCases.restore(closedTab)

        assertEquals(2, sessionManager.sessions.size)
        assertEquals(2, store.state.tabs.size)
        assertEquals("wikipedia", sessionManager.selectedSessionOrThrow.id)
        assertEquals("wikipedia", store.state.selectedTabId)
    }

    @Test
    fun `selectOrAddTab selects already existing tab`() {
        val store = BrowserStore()
        val sessionManager = SessionManager(engine = mock(), store = store)
        val useCases = TabsUseCases(store, sessionManager)

        sessionManager.add(Session("https://www.mozilla.org", id = "mozilla"))
        sessionManager.add(Session("https://firefox.com", id = "firefox"))
        sessionManager.add(Session("https://getpocket.com", id = "pocket"))

        assertEquals("mozilla", store.state.selectedTabId)
        assertEquals(3, store.state.tabs.size)

        useCases.selectOrAddTab("https://getpocket.com")

        assertEquals("pocket", store.state.selectedTabId)
        assertEquals(3, store.state.tabs.size)
    }

    @Test
    fun `selectOrAddTab adds new tab if no matching existing tab could be found`() {
        val store = BrowserStore()
        val sessionManager = SessionManager(engine = mock(), store = store)
        val useCases = TabsUseCases(store, sessionManager)

        sessionManager.add(Session("https://www.mozilla.org", id = "mozilla"))
        sessionManager.add(Session("https://firefox.com", id = "firefox"))
        sessionManager.add(Session("https://getpocket.com", id = "pocket"))

        assertEquals("mozilla", store.state.selectedTabId)
        assertEquals(3, store.state.tabs.size)

        useCases.selectOrAddTab("https://youtube.com")

        assertNotEquals("mozilla", store.state.selectedTabId)
        assertEquals(4, store.state.tabs.size)
        assertEquals("https://youtube.com", store.state.tabs.last().content.url)
        assertEquals("https://youtube.com", store.state.selectedTab!!.content.url)
    }

    @Test
    fun `duplicateTab creates a duplicate of the given tab`() {
        val store = BrowserStore()
        val sessionManager = SessionManager(engine = mock(), store = store)

        val useCases = TabsUseCases(store, sessionManager)

        sessionManager.add(Session("https://www.mozilla.org", id = "mozilla"))

        val engineSessionState: EngineSessionState = mock()
        store.dispatch(
            EngineAction.UpdateEngineSessionStateAction("mozilla", engineSessionState)
        ).joinBlocking()

        val tab = store.state.findTab("mozilla")!!
        useCases.duplicateTab.invoke(tab)

        assertEquals(2, store.state.tabs.size)

        assertEquals("mozilla", store.state.tabs[0].id)
        assertNotEquals("mozilla", store.state.tabs[1].id)
        assertFalse(store.state.tabs[0].content.private)
        assertFalse(store.state.tabs[1].content.private)
        assertEquals("https://www.mozilla.org", store.state.tabs[0].content.url)
        assertEquals("https://www.mozilla.org", store.state.tabs[1].content.url)
        assertEquals(engineSessionState, store.state.tabs[0].engineState.engineSessionState)
        assertEquals(engineSessionState, store.state.tabs[1].engineState.engineSessionState)
        assertNull(store.state.tabs[0].parentId)
        assertEquals("mozilla", store.state.tabs[1].parentId)
    }

    @Test
    fun `duplicateTab creates duplicates of private tabs`() {
        val store = BrowserStore()
        val sessionManager = SessionManager(engine = mock(), store = store)

        val useCases = TabsUseCases(store, sessionManager)

        sessionManager.add(Session("https://www.mozilla.org", id = "mozilla", private = true))

        val tab = store.state.findTab("mozilla")!!
        useCases.duplicateTab.invoke(tab)

        assertEquals(2, store.state.tabs.size)
        assertTrue(store.state.tabs[0].content.private)
        assertTrue(store.state.tabs[1].content.private)
    }

    @Test
    fun `duplicateTab keeps contextId`() {
        val store = BrowserStore()
        val sessionManager = SessionManager(engine = mock(), store = store)

        val useCases = TabsUseCases(store, sessionManager)

        sessionManager.add(Session("https://www.mozilla.org", id = "mozilla", contextId = "work"))

        val tab = store.state.findTab("mozilla")!!
        useCases.duplicateTab.invoke(tab)

        assertEquals(2, store.state.tabs.size)
        assertEquals("work", store.state.tabs[0].contextId)
        assertEquals("work", store.state.tabs[1].contextId)
    }
}
