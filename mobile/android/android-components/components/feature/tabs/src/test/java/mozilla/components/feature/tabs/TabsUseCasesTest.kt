/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs

import kotlinx.coroutines.runBlocking
import mozilla.components.browser.session.storage.SessionStorage
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.engine.EngineMiddleware
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.selector.findTabByUrl
import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.state.recover.RecoverableTab
import mozilla.components.browser.state.state.recover.toRecoverableTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags
import mozilla.components.concept.engine.EngineSessionState
import mozilla.components.concept.storage.HistoryMetadataKey
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
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mockito.ArgumentMatchers.anyBoolean
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito.never
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

const val DAY_IN_MS = 24 * 60 * 60 * 1000L

class TabsUseCasesTest {

    private lateinit var store: BrowserStore
    private lateinit var tabsUseCases: TabsUseCases
    private lateinit var engine: Engine
    private lateinit var engineSession: EngineSession

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val dispatcher = coroutinesTestRule.testDispatcher

    @Before
    fun setup() {
        engineSession = mock()
        engine = mock()

        whenever(engine.createSession(anyBoolean(), any())).thenReturn(engineSession)
        store = BrowserStore(
            middleware = EngineMiddleware.create(
                engine = engine
            )
        )
        tabsUseCases = TabsUseCases(store)
    }

    @Test
    fun `SelectTabUseCase - tab is marked as selected in store`() {
        val tab = createTab("https://mozilla.org")
        val otherTab = createTab("https://firefox.com")
        store.dispatch(TabListAction.AddTabAction(otherTab)).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()

        assertEquals(otherTab.id, store.state.selectedTabId)
        assertEquals(otherTab, store.state.selectedTab)

        tabsUseCases.selectTab(tab.id)
        store.waitUntilIdle()
        assertEquals(tab.id, store.state.selectedTabId)
        assertEquals(tab, store.state.selectedTab)
    }

    @Test
    fun `RemoveTabUseCase - session will be removed from store`() {
        val tab = createTab("https://mozilla.org")
        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()
        assertEquals(1, store.state.tabs.size)

        tabsUseCases.removeTab(tab.id)
        store.waitUntilIdle()
        assertEquals(0, store.state.tabs.size)
    }

    @Test
    fun `RemoveTabUseCase - remove by ID and select parent if it exists`() {
        val parentTab = createTab("https://firefox.com")
        store.dispatch(TabListAction.AddTabAction(parentTab)).joinBlocking()

        val tab = createTab("https://mozilla.org", parent = parentTab)
        store.dispatch(TabListAction.AddTabAction(tab, select = true)).joinBlocking()
        assertEquals(2, store.state.tabs.size)
        assertEquals(tab.id, store.state.selectedTabId)

        tabsUseCases.removeTab(tab.id, selectParentIfExists = true)
        store.waitUntilIdle()
        assertEquals(1, store.state.tabs.size)
        assertEquals(parentTab.id, store.state.selectedTabId)
    }

    @Test
    fun `RemoveTabsUseCase - list of sessions can be removed`() {
        val tab = createTab("https://mozilla.org")
        val otherTab = createTab("https://firefox.com")
        store.dispatch(TabListAction.AddTabAction(otherTab)).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()

        assertEquals(otherTab.id, store.state.selectedTabId)
        assertEquals(otherTab, store.state.selectedTab)

        tabsUseCases.removeTabs(listOf(tab.id, otherTab.id))
        store.waitUntilIdle()
        assertEquals(0, store.state.tabs.size)
    }

    @Test
    fun `AddNewTabUseCase - session will be added to store`() {
        tabsUseCases.addTab("https://www.mozilla.org")

        store.waitUntilIdle()
        assertEquals(1, store.state.tabs.size)
        assertEquals("https://www.mozilla.org", store.state.tabs[0].content.url)
        assertFalse(store.state.tabs[0].content.private)
    }

    @Test
    fun `AddNewTabUseCase - private session will be added to store`() {
        tabsUseCases.addTab("https://www.mozilla.org", private = true)

        store.waitUntilIdle()
        assertEquals(1, store.state.tabs.size)
        assertEquals("https://www.mozilla.org", store.state.tabs[0].content.url)
        assertTrue(store.state.tabs[0].content.private)
    }

    @Test
    fun `AddNewTabUseCase will not load URL if flag is set to false`() {
        tabsUseCases.addTab("https://www.mozilla.org", startLoading = false)

        store.waitUntilIdle()
        assertEquals(1, store.state.tabs.size)
        assertEquals("https://www.mozilla.org", store.state.tabs[0].content.url)
        verify(engineSession, never()).loadUrl(anyString(), any(), any(), any())
    }

    @Test
    fun `AddNewTabUseCase will load URL if flag is set to true`() {
        tabsUseCases.addTab("https://www.mozilla.org", startLoading = true)

        // Wait for CreateEngineSessionAction and middleware
        store.waitUntilIdle()
        dispatcher.advanceUntilIdle()

        // Wait for LinkEngineSessionAction and middleware
        store.waitUntilIdle()
        dispatcher.advanceUntilIdle()

        assertEquals(1, store.state.tabs.size)
        assertEquals("https://www.mozilla.org", store.state.tabs[0].content.url)
        verify(engineSession, times(1)).loadUrl("https://www.mozilla.org")
    }

    @Test
    fun `AddNewTabUseCase forwards load flags to engine`() {
        tabsUseCases.addTab.invoke("https://www.mozilla.org", flags = LoadUrlFlags.external(), startLoading = true)

        store.waitUntilIdle()
        dispatcher.advanceUntilIdle()

        store.waitUntilIdle()
        dispatcher.advanceUntilIdle()

        assertEquals(1, store.state.tabs.size)
        assertEquals("https://www.mozilla.org", store.state.tabs[0].content.url)
        verify(engineSession, times(1)).loadUrl("https://www.mozilla.org", null, LoadUrlFlags.external(), null)
    }

    @Test
    fun `AddNewTabUseCase uses provided engine session`() {
        val session: EngineSession = mock()
        tabsUseCases.addTab.invoke(
            "https://www.mozilla.org",
            flags = LoadUrlFlags.external(),
            startLoading = true,
            engineSession = session
        )

        store.waitUntilIdle()

        assertEquals(1, store.state.tabs.size)
        assertEquals("https://www.mozilla.org", store.state.tabs[0].content.url)
        assertSame(session, store.state.tabs[0].engineState.engineSession)
    }

    @Test
    fun `AddNewTabUseCase uses provided contextId`() {
        val contextId = "1"
        tabsUseCases.addTab.invoke(
            "https://www.mozilla.org",
            flags = LoadUrlFlags.external(),
            startLoading = true,
            contextId = contextId
        )

        store.waitUntilIdle()

        assertEquals(1, store.state.tabs.size)
        assertEquals("https://www.mozilla.org", store.state.tabs[0].content.url)
        assertEquals(contextId, store.state.tabs[0].contextId)
    }

    @Test
    @Suppress("DEPRECATION")
    fun `AddNewPrivateTabUseCase will not load URL if flag is set to false`() {
        tabsUseCases.addPrivateTab("https://www.mozilla.org", startLoading = false)

        store.waitUntilIdle()
        assertEquals(1, store.state.tabs.size)
        assertEquals("https://www.mozilla.org", store.state.tabs[0].content.url)
        verify(engineSession, never()).loadUrl(anyString(), any(), any(), any())
    }

    @Test
    @Suppress("DEPRECATION")
    fun `AddNewPrivateTabUseCase will load URL if flag is set to true`() {
        tabsUseCases.addPrivateTab("https://www.mozilla.org", startLoading = true)

        // Wait for CreateEngineSessionAction and middleware
        store.waitUntilIdle()
        dispatcher.advanceUntilIdle()

        // Wait for LinkEngineSessionAction and middleware
        store.waitUntilIdle()
        dispatcher.advanceUntilIdle()

        assertEquals(1, store.state.tabs.size)
        assertEquals("https://www.mozilla.org", store.state.tabs[0].content.url)
        verify(engineSession, times(1)).loadUrl("https://www.mozilla.org")
    }

    @Test
    @Suppress("DEPRECATION")
    fun `AddNewPrivateTabUseCase forwards load flags to engine`() {
        tabsUseCases.addPrivateTab.invoke("https://www.mozilla.org", flags = LoadUrlFlags.external(), startLoading = true)

        store.waitUntilIdle()
        dispatcher.advanceUntilIdle()

        store.waitUntilIdle()
        dispatcher.advanceUntilIdle()

        assertEquals(1, store.state.tabs.size)
        assertEquals("https://www.mozilla.org", store.state.tabs[0].content.url)
        verify(engineSession, times(1)).loadUrl("https://www.mozilla.org", null, LoadUrlFlags.external(), null)
    }

    @Test
    @Suppress("DEPRECATION")
    fun `AddNewPrivateTabUseCase uses provided engine session`() {
        val session: EngineSession = mock()
        tabsUseCases.addPrivateTab.invoke(
            "https://www.mozilla.org",
            flags = LoadUrlFlags.external(),
            startLoading = true,
            engineSession = session
        )

        store.waitUntilIdle()

        assertEquals(1, store.state.tabs.size)
        assertEquals("https://www.mozilla.org", store.state.tabs[0].content.url)
        assertSame(session, store.state.tabs[0].engineState.engineSession)
    }

    @Test
    fun `RemoveAllTabsUseCase will remove all sessions`() {
        val tab = createTab("https://mozilla.org")
        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()
        assertEquals(1, store.state.tabs.size)

        val tab2 = createTab("https://firefox.com", private = true)
        store.dispatch(TabListAction.AddTabAction(tab2)).joinBlocking()
        assertEquals(2, store.state.tabs.size)

        tabsUseCases.removeAllTabs()
        store.waitUntilIdle()
        assertEquals(0, store.state.tabs.size)
    }

    @Test
    fun `RemoveNormalTabsUseCase and RemovePrivateTabsUseCase will remove sessions for particular type of tabs private or normal`() {
        val tab = createTab("https://mozilla.org")
        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()
        assertEquals(1, store.state.tabs.size)

        val privateTab = createTab("https://firefox.com", private = true)
        store.dispatch(TabListAction.AddTabAction(privateTab)).joinBlocking()
        assertEquals(2, store.state.tabs.size)

        tabsUseCases.removeNormalTabs()
        store.waitUntilIdle()
        assertEquals(1, store.state.tabs.size)

        tabsUseCases.removePrivateTabs()
        store.waitUntilIdle()
        assertEquals(0, store.state.tabs.size)
    }

    @Test
    fun `RestoreUseCase - filters based on tab timeout`() = runBlocking {
        val useCases = TabsUseCases(BrowserStore())

        val now = System.currentTimeMillis()
        val twoDays = now - 2 * DAY_IN_MS
        val threeDays = now - 3 * DAY_IN_MS
        val tabs = listOf(
            createTab("https://mozilla.org", lastAccess = 0).toRecoverableTab(),
            createTab("https://mozilla.org", lastAccess = now).toRecoverableTab(),
            createTab("https://firefox.com", lastAccess = twoDays, createdAt = threeDays).toRecoverableTab(),
            createTab("https://getpocket.com", lastAccess = threeDays, createdAt = threeDays).toRecoverableTab()
        )

        val sessionStorage: SessionStorage = mock()
        useCases.restore(sessionStorage, tabTimeoutInMs = DAY_IN_MS)

        val predicateCaptor = argumentCaptor<(RecoverableTab) -> Boolean>()
        verify(sessionStorage).restore(predicateCaptor.capture())

        // Only the first two tab should be restored, all others "timed out."
        val restoredTabs = tabs.filter(predicateCaptor.value)
        assertEquals(2, restoredTabs.size)
        assertEquals(tabs.first(), restoredTabs.first())
    }

    @Test
    fun `selectOrAddTab selects already existing tab`() {
        val tab = createTab("https://mozilla.org")
        val otherTab = createTab("https://firefox.com")

        store.dispatch(TabListAction.AddTabAction(otherTab)).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()

        assertEquals(otherTab.id, store.state.selectedTabId)
        assertEquals(otherTab, store.state.selectedTab)
        assertEquals(2, store.state.tabs.size)

        val tabID = tabsUseCases.selectOrAddTab(tab.content.url)
        store.waitUntilIdle()

        assertEquals(2, store.state.tabs.size)
        assertEquals(tab.id, store.state.selectedTabId)
        assertEquals(tab, store.state.selectedTab)
        assertEquals(tab.id, tabID)
    }

    @Test
    fun `selectOrAddTab selects already existing tab with matching historyMetadata`() {
        val historyMetadata = HistoryMetadataKey(
            url = "https://mozilla.org",
            referrerUrl = "https://firefox.com"
        )

        val tab = createTab("https://mozilla.org", historyMetadata = historyMetadata)
        val otherTab = createTab("https://firefox.com")

        store.dispatch(TabListAction.AddTabAction(otherTab)).joinBlocking()
        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()

        assertEquals(otherTab.id, store.state.selectedTabId)
        assertEquals(otherTab, store.state.selectedTab)
        assertEquals(2, store.state.tabs.size)

        val tabID = tabsUseCases.selectOrAddTab(tab.content.url, historyMetadata = historyMetadata)
        store.waitUntilIdle()

        assertEquals(2, store.state.tabs.size)
        assertEquals(tab.id, store.state.selectedTabId)
        assertEquals(tab, store.state.selectedTab)
        assertEquals(tab.id, tabID)
    }

    @Test
    fun `selectOrAddTab adds new tab if no matching existing tab could be found`() {
        val tab = createTab("https://mozilla.org")

        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()

        assertEquals(tab.id, store.state.selectedTabId)
        assertEquals(tab, store.state.selectedTab)
        assertEquals(1, store.state.tabs.size)

        val tabID = tabsUseCases.selectOrAddTab("https://firefox.com")
        store.waitUntilIdle()

        assertEquals(2, store.state.tabs.size)
        assertNotNull(store.state.findTabByUrl("https://firefox.com"))
        assertEquals(store.state.selectedTabId, tabID)
    }

    @Test
    fun `selectOrAddTab adds new tab if no matching existing history metadata could be found`() {
        val tab = createTab("https://mozilla.org")
        val historyMetadata = HistoryMetadataKey(
            url = "https://mozilla.org",
            referrerUrl = "https://firefox.com"
        )

        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()

        assertEquals(tab.id, store.state.selectedTabId)
        assertEquals(tab, store.state.selectedTab)
        assertEquals(1, store.state.tabs.size)

        val tabID =
            tabsUseCases.selectOrAddTab("https://firefox.com", historyMetadata = historyMetadata)
        store.waitUntilIdle()

        assertEquals(2, store.state.tabs.size)
        assertNotNull(store.state.findTabByUrl("https://firefox.com"))
        assertEquals(store.state.selectedTabId, tabID)
    }

    @Test
    fun `duplicateTab creates a duplicate of the given tab`() {
        store.dispatch(
            TabListAction.AddTabAction(
                createTab(id = "mozilla", url = "https://www.mozilla.org")
            )
        ).joinBlocking()
        assertEquals(1, store.state.tabs.size)

        val engineSessionState: EngineSessionState = mock()
        store.dispatch(
            EngineAction.UpdateEngineSessionStateAction("mozilla", engineSessionState)
        ).joinBlocking()

        val tab = store.state.findTab("mozilla")!!
        tabsUseCases.duplicateTab.invoke(tab)
        store.waitUntilIdle()
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
        val tab = createTab(id = "mozilla", url = "https://www.mozilla.org", private = true)
        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()
        tabsUseCases.duplicateTab.invoke(tab)
        store.waitUntilIdle()

        assertEquals(2, store.state.tabs.size)
        assertTrue(store.state.tabs[0].content.private)
        assertTrue(store.state.tabs[1].content.private)
    }

    @Test
    fun `duplicateTab keeps contextId`() {
        val tab = createTab(id = "mozilla", url = "https://www.mozilla.org", contextId = "work")
        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()
        tabsUseCases.duplicateTab.invoke(tab)
        store.waitUntilIdle()

        assertEquals(2, store.state.tabs.size)
        assertEquals("work", store.state.tabs[0].contextId)
        assertEquals("work", store.state.tabs[1].contextId)
    }
}
