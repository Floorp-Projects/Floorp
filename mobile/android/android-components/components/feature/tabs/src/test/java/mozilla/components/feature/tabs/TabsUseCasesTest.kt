/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.SessionManager
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.state.SessionState.Source
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags
import mozilla.components.support.test.argumentCaptor
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class TabsUseCasesTest {

    @Test
    fun `SelectTabUseCase - session will be selected in session manager`() {
        val sessionManager: SessionManager = mock()
        val useCases = TabsUseCases(BrowserStore(), sessionManager)

        val session = Session("A")
        useCases.selectTab(session)

        verify(sessionManager).select(session)
    }

    @Test
    fun `RemoveTabUseCase - session will be removed in session manager`() {
        val sessionManager: SessionManager = mock()
        val useCases = TabsUseCases(BrowserStore(), sessionManager)

        val session = Session("A")
        useCases.removeTab(session)

        verify(sessionManager).remove(session)
    }

    @Test
    fun `RemoveTabUseCase - session can be removed by ID`() {
        val sessionManager = spy(SessionManager(mock()))
        val useCases = TabsUseCases(BrowserStore(), sessionManager)

        val session = Session(id = "test", initialUrl = "http://mozilla.org")
        sessionManager.remove(session)
        useCases.removeTab(session.id)

        verify(sessionManager).remove(session)
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
        val sessionManager: SessionManager = mock()
        val store: BrowserStore = mock()
        val useCases = TabsUseCases(store, sessionManager)

        useCases.addTab.invoke("https://www.mozilla.org", LoadUrlFlags.select(LoadUrlFlags.EXTERNAL))
        val actionCaptor = argumentCaptor<EngineAction.LoadUrlAction>()
        verify(store).dispatch(actionCaptor.capture())
        assertEquals("https://www.mozilla.org", actionCaptor.value.url)
        assertEquals(LoadUrlFlags.select(LoadUrlFlags.EXTERNAL), actionCaptor.value.flags)
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
    fun `RemoveAllTabsOfTypeUseCase will remove sessions for particular type of tabs private or normal`() {
        val sessionManager = spy(SessionManager(mock()))
        val useCases = TabsUseCases(BrowserStore(), sessionManager)

        val session1 = Session("https://www.mozilla.org")
        session1.customTabConfig = mock()
        sessionManager.add(session1)
        useCases.addPrivateTab("https://www.mozilla.org")
        useCases.addTab("https://www.mozilla.org")
        assertEquals(3, sessionManager.size)

        useCases.removeAllTabsOfType(private = false)
        assertEquals(2, sessionManager.all.size)

        useCases.addPrivateTab("https://www.mozilla.org")
        useCases.addTab("https://www.mozilla.org")
        useCases.addTab("https://www.mozilla.org")
        assertEquals(5, sessionManager.size)

        useCases.removeAllTabsOfType(private = true)
        assertEquals(3, sessionManager.size)

        useCases.removeAllTabsOfType(private = false)
        assertEquals(1, sessionManager.size)

        assertTrue(sessionManager.all[0].isCustomTabSession())
    }
}
