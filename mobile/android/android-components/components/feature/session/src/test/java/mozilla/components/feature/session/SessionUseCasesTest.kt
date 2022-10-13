/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import kotlinx.coroutines.test.runTest
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.CrashAction
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.engine.EngineMiddleware
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createCustomTab
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSession.LoadUrlFlags
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.middleware.CaptureActionsMiddleware
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class SessionUseCasesTest {
    private lateinit var middleware: CaptureActionsMiddleware<BrowserState, BrowserAction>
    private lateinit var store: BrowserStore
    private lateinit var useCases: SessionUseCases
    private lateinit var engineSession: EngineSession
    private lateinit var childEngineSession: EngineSession

    @Before
    fun setUp() {
        engineSession = mock()
        childEngineSession = mock()
        middleware = CaptureActionsMiddleware()
        store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(
                    createTab(
                        url = "https://mozilla.org",
                        id = "mozilla",
                        engineSession = engineSession,
                    ),
                    createTab(
                        url = "https://bugzilla.com",
                        id = "bugzilla",
                        engineSession = childEngineSession,
                        parentId = "mozilla",
                    ),
                ),
                selectedTabId = "mozilla",
            ),
            middleware = listOf(middleware) + EngineMiddleware.create(engine = mock()),
        )
        useCases = SessionUseCases(store)
    }

    @Test
    fun loadUrlWithEngineSession() {
        useCases.loadUrl("https://getpocket.com")
        store.waitUntilIdle()
        middleware.assertNotDispatched(EngineAction.LoadUrlAction::class)
        verify(engineSession).loadUrl(url = "https://getpocket.com")
        middleware.assertLastAction(EngineAction.OptimizedLoadUrlTriggeredAction::class) { action ->
            assertEquals("mozilla", action.tabId)
            assertEquals("https://getpocket.com", action.url)
        }

        useCases.loadUrl("https://www.mozilla.org", LoadUrlFlags.select(LoadUrlFlags.EXTERNAL))
        store.waitUntilIdle()
        middleware.assertNotDispatched(EngineAction.LoadUrlAction::class)
        verify(engineSession).loadUrl(
            url = "https://www.mozilla.org",
            flags = LoadUrlFlags.select(LoadUrlFlags.EXTERNAL),
        )
        middleware.assertLastAction(EngineAction.OptimizedLoadUrlTriggeredAction::class) { action ->
            assertEquals("mozilla", action.tabId)
            assertEquals("https://www.mozilla.org", action.url)
            assertEquals(LoadUrlFlags.select(LoadUrlFlags.EXTERNAL), action.flags)
        }

        useCases.loadUrl("https://firefox.com", store.state.selectedTabId)
        store.waitUntilIdle()
        middleware.assertNotDispatched(EngineAction.LoadUrlAction::class)
        verify(engineSession).loadUrl(url = "https://firefox.com")
        middleware.assertLastAction(EngineAction.OptimizedLoadUrlTriggeredAction::class) { action ->
            assertEquals("mozilla", action.tabId)
            assertEquals("https://firefox.com", action.url)
        }

        useCases.loadUrl.invoke(
            "https://developer.mozilla.org",
            store.state.selectedTabId,
            LoadUrlFlags.select(LoadUrlFlags.BYPASS_PROXY),
        )
        store.waitUntilIdle()
        middleware.assertNotDispatched(EngineAction.LoadUrlAction::class)
        verify(engineSession).loadUrl(
            url = "https://developer.mozilla.org",
            flags = LoadUrlFlags.select(LoadUrlFlags.BYPASS_PROXY),
        )
        middleware.assertLastAction(EngineAction.OptimizedLoadUrlTriggeredAction::class) { action ->
            assertEquals("mozilla", action.tabId)
            assertEquals("https://developer.mozilla.org", action.url)
            assertEquals(LoadUrlFlags.select(LoadUrlFlags.BYPASS_PROXY), action.flags)
        }

        useCases.loadUrl.invoke(
            "https://www.mozilla.org/en-CA/firefox/browsers/mobile/",
            "bugzilla",
        )
        store.waitUntilIdle()
        middleware.assertNotDispatched(EngineAction.LoadUrlAction::class)
        verify(childEngineSession).loadUrl(
            url = "https://www.mozilla.org/en-CA/firefox/browsers/mobile/",
            parent = engineSession,
        )
        middleware.assertLastAction(EngineAction.OptimizedLoadUrlTriggeredAction::class) { action ->
            assertEquals("bugzilla", action.tabId)
            assertEquals("https://www.mozilla.org/en-CA/firefox/browsers/mobile/", action.url)
        }
    }

    @Test
    fun loadUrlWithoutEngineSession() {
        store.dispatch(EngineAction.UnlinkEngineSessionAction("mozilla")).joinBlocking()

        useCases.loadUrl("https://getpocket.com")
        store.waitUntilIdle()

        middleware.assertLastAction(EngineAction.LoadUrlAction::class) { action ->
            assertEquals("mozilla", action.tabId)
            assertEquals("https://getpocket.com", action.url)
        }

        useCases.loadUrl("https://www.mozilla.org", LoadUrlFlags.select(LoadUrlFlags.EXTERNAL))
        store.waitUntilIdle()

        middleware.assertLastAction(EngineAction.LoadUrlAction::class) { action ->
            assertEquals("mozilla", action.tabId)
            assertEquals("https://www.mozilla.org", action.url)
            assertEquals(LoadUrlFlags.select(LoadUrlFlags.EXTERNAL), action.flags)
        }

        useCases.loadUrl("https://firefox.com", store.state.selectedTabId)
        store.waitUntilIdle()

        middleware.assertLastAction(EngineAction.LoadUrlAction::class) { action ->
            assertEquals("mozilla", action.tabId)
            assertEquals("https://firefox.com", action.url)
        }

        useCases.loadUrl.invoke(
            "https://developer.mozilla.org",
            store.state.selectedTabId,
            LoadUrlFlags.select(LoadUrlFlags.BYPASS_PROXY),
        )
        store.waitUntilIdle()

        middleware.assertLastAction(EngineAction.LoadUrlAction::class) { action ->
            assertEquals("mozilla", action.tabId)
            assertEquals("https://developer.mozilla.org", action.url)
            assertEquals(LoadUrlFlags.select(LoadUrlFlags.BYPASS_PROXY), action.flags)
        }
    }

    @Test
    fun loadData() {
        useCases.loadData("<html><body></body></html>", "text/html")
        store.waitUntilIdle()
        middleware.assertLastAction(EngineAction.LoadDataAction::class) { action ->
            assertEquals("mozilla", action.tabId)
            assertEquals("<html><body></body></html>", action.data)
            assertEquals("text/html", action.mimeType)
            assertEquals("UTF-8", action.encoding)
        }

        useCases.loadData(
            "Should load in WebView",
            "text/plain",
            tabId = store.state.selectedTabId,
        )
        store.waitUntilIdle()
        middleware.assertLastAction(EngineAction.LoadDataAction::class) { action ->
            assertEquals("mozilla", action.tabId)
            assertEquals("Should load in WebView", action.data)
            assertEquals("text/plain", action.mimeType)
            assertEquals("UTF-8", action.encoding)
        }

        useCases.loadData(
            "Should also load in WebView",
            "text/plain",
            "base64",
            store.state.selectedTabId,
        )
        store.waitUntilIdle()
        middleware.assertLastAction(EngineAction.LoadDataAction::class) { action ->
            assertEquals("mozilla", action.tabId)
            assertEquals("Should also load in WebView", action.data)
            assertEquals("text/plain", action.mimeType)
            assertEquals("base64", action.encoding)
        }
    }

    @Test
    fun reload() {
        useCases.reload()
        store.waitUntilIdle()

        middleware.assertLastAction(EngineAction.ReloadAction::class) { action ->
            assertEquals("mozilla", action.tabId)
        }

        useCases.reload(store.state.selectedTabId, LoadUrlFlags.select(LoadUrlFlags.EXTERNAL))
        store.waitUntilIdle()

        middleware.assertLastAction(EngineAction.ReloadAction::class) { action ->
            assertEquals("mozilla", action.tabId)
            assertEquals(LoadUrlFlags.select(LoadUrlFlags.EXTERNAL), action.flags)
        }
    }

    @Test
    fun reloadBypassCache() {
        val flags = LoadUrlFlags.select(LoadUrlFlags.BYPASS_CACHE)
        useCases.reload(store.state.selectedTabId, flags = flags)
        store.waitUntilIdle()

        middleware.assertLastAction(EngineAction.ReloadAction::class) { action ->
            assertEquals("mozilla", action.tabId)
            assertEquals(flags, action.flags)
        }
    }

    @Test
    fun stopLoading() = runTest {
        useCases.stopLoading()
        store.waitUntilIdle()
        verify(engineSession).stopLoading()

        useCases.stopLoading(store.state.selectedTabId)
        store.waitUntilIdle()
        verify(engineSession, times(2)).stopLoading()
    }

    @Test
    fun goBack() {
        useCases.goBack(null)
        store.waitUntilIdle()
        middleware.assertNotDispatched(EngineAction.GoBackAction::class)

        useCases.goBack(store.state.selectedTabId)
        store.waitUntilIdle()
        middleware.assertLastAction(EngineAction.GoBackAction::class) { action ->
            assertEquals("mozilla", action.tabId)
            assertTrue(action.userInteraction)
        }
        middleware.reset()

        useCases.goBack(userInteraction = false)
        store.waitUntilIdle()
        middleware.assertLastAction(EngineAction.GoBackAction::class) { action ->
            assertEquals("mozilla", action.tabId)
            assertFalse(action.userInteraction)
        }
    }

    @Test
    fun goForward() {
        useCases.goForward(null)
        store.waitUntilIdle()
        middleware.assertNotDispatched(EngineAction.GoForwardAction::class)

        useCases.goForward(store.state.selectedTabId)
        store.waitUntilIdle()
        middleware.assertLastAction(EngineAction.GoForwardAction::class) { action ->
            assertEquals("mozilla", action.tabId)
            assertTrue(action.userInteraction)
        }
        middleware.reset()

        useCases.goForward(userInteraction = false)
        store.waitUntilIdle()
        middleware.assertLastAction(EngineAction.GoForwardAction::class) { action ->
            assertEquals("mozilla", action.tabId)
            assertFalse(action.userInteraction)
        }
    }

    @Test
    fun goToHistoryIndex() {
        useCases.goToHistoryIndex(tabId = null, index = 0)
        store.waitUntilIdle()
        middleware.assertNotDispatched(EngineAction.GoToHistoryIndexAction::class)

        useCases.goToHistoryIndex(tabId = "test", index = 5)
        store.waitUntilIdle()
        middleware.assertLastAction(EngineAction.GoToHistoryIndexAction::class) { action ->
            assertEquals("test", action.tabId)
            assertEquals(5, action.index)
        }

        useCases.goToHistoryIndex(index = 10)
        store.waitUntilIdle()
        middleware.assertLastAction(EngineAction.GoToHistoryIndexAction::class) { action ->
            assertEquals("mozilla", action.tabId)
            assertEquals(10, action.index)
        }
    }

    @Test
    fun requestDesktopSite() {
        useCases.requestDesktopSite(true, store.state.selectedTabId)
        store.waitUntilIdle()
        middleware.assertLastAction(EngineAction.ToggleDesktopModeAction::class) { action ->
            assertEquals("mozilla", action.tabId)
            assertTrue(action.enable)
        }

        useCases.requestDesktopSite(false)
        store.waitUntilIdle()
        middleware.assertLastAction(EngineAction.ToggleDesktopModeAction::class) { action ->
            assertEquals("mozilla", action.tabId)
            assertFalse(action.enable)
        }
        useCases.requestDesktopSite(true, store.state.selectedTabId)
        store.waitUntilIdle()
        middleware.assertLastAction(EngineAction.ToggleDesktopModeAction::class) { action ->
            assertEquals("mozilla", action.tabId)
            assertTrue(action.enable)
        }
        useCases.requestDesktopSite(false, store.state.selectedTabId)
        store.waitUntilIdle()
        middleware.assertLastAction(EngineAction.ToggleDesktopModeAction::class) { action ->
            assertEquals("mozilla", action.tabId)
            assertFalse(action.enable)
        }
    }

    @Test
    fun exitFullscreen() {
        useCases.exitFullscreen(store.state.selectedTabId)
        store.waitUntilIdle()
        middleware.assertLastAction(EngineAction.ExitFullScreenModeAction::class) { action ->
            assertEquals("mozilla", action.tabId)
        }
        middleware.reset()

        useCases.exitFullscreen()
        store.waitUntilIdle()
        middleware.assertLastAction(EngineAction.ExitFullScreenModeAction::class) { action ->
            assertEquals("mozilla", action.tabId)
        }
    }

    @Test
    fun `LoadUrlUseCase will invoke onNoSession lambda if no selected session exists`() {
        var createdTab: TabSessionState? = null
        var tabCreatedForUrl: String? = null

        store.dispatch(TabListAction.RemoveAllTabsAction()).joinBlocking()

        val loadUseCase = SessionUseCases.DefaultLoadUrlUseCase(store) { url ->
            tabCreatedForUrl = url
            createTab(url).also { createdTab = it }
        }

        loadUseCase("https://www.example.com")
        store.waitUntilIdle()

        assertEquals("https://www.example.com", tabCreatedForUrl)
        assertNotNull(createdTab)

        middleware.assertLastAction(EngineAction.LoadUrlAction::class) { action ->
            assertEquals(createdTab!!.id, action.tabId)
            assertEquals(tabCreatedForUrl, action.url)
        }
    }

    @Test
    fun `LoadDataUseCase will invoke onNoSession lambda if no selected session exists`() {
        var createdTab: TabSessionState? = null
        var tabCreatedForUrl: String? = null

        store.dispatch(TabListAction.RemoveAllTabsAction()).joinBlocking()
        store.waitUntilIdle()

        val loadUseCase = SessionUseCases.LoadDataUseCase(store) { url ->
            tabCreatedForUrl = url
            createTab(url).also { createdTab = it }
        }

        loadUseCase("Hello", mimeType = "text/plain", encoding = "UTF-8")
        store.waitUntilIdle()

        assertEquals("about:blank", tabCreatedForUrl)
        assertNotNull(createdTab)

        middleware.assertLastAction(EngineAction.LoadDataAction::class) { action ->
            assertEquals(createdTab!!.id, action.tabId)
            assertEquals("Hello", action.data)
            assertEquals("text/plain", action.mimeType)
            assertEquals("UTF-8", action.encoding)
        }
    }

    @Test
    fun `CrashRecoveryUseCase will restore specified session`() {
        useCases.crashRecovery.invoke(listOf("mozilla"))
        store.waitUntilIdle()

        middleware.assertLastAction(CrashAction.RestoreCrashedSessionAction::class) { action ->
            assertEquals("mozilla", action.tabId)
        }
    }

    @Test
    fun `CrashRecoveryUseCase will restore list of crashed sessions`() {
        val store = spy(
            BrowserStore(
                middleware = listOf(middleware),
                initialState = BrowserState(
                    tabs = listOf(
                        createTab(url = "https://wwww.mozilla.org", id = "tab1", crashed = true),
                    ),
                    customTabs = listOf(
                        createCustomTab(
                            "https://wwww.mozilla.org",
                            id = "customTab1",
                            crashed = true,
                        ),
                    ),
                ),
            ),
        )
        val useCases = SessionUseCases(store)

        useCases.crashRecovery.invoke()
        store.waitUntilIdle()

        middleware.assertFirstAction(CrashAction.RestoreCrashedSessionAction::class) { action ->
            assertEquals("tab1", action.tabId)
        }

        middleware.assertLastAction(CrashAction.RestoreCrashedSessionAction::class) { action ->
            assertEquals("customTab1", action.tabId)
        }
    }

    @Test
    fun `PurgeHistoryUseCase dispatches PurgeHistory action`() {
        useCases.purgeHistory()
        store.waitUntilIdle()

        middleware.findFirstAction(EngineAction.PurgeHistoryAction::class)
    }

    @Test
    fun `UpdateLastAccessUseCase sets timestamp`() {
        val tab = createTab("https://firefox.com")
        val otherTab = createTab("https://example.com")
        val customTab = createCustomTab("https://getpocket.com")
        store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(tab, otherTab),
                customTabs = listOf(customTab),
            ),
        )
        useCases = SessionUseCases(store)

        // Make sure use case doesn't crash for custom tab and non-existent tab
        useCases.updateLastAccess(customTab.id)
        store.waitUntilIdle()
        assertEquals(0L, store.state.findTab(tab.id)?.lastAccess)

        // Update last access for a specific tab with default value
        useCases.updateLastAccess(tab.id)
        store.waitUntilIdle()
        assertNotEquals(0L, store.state.findTab(tab.id)?.lastAccess)

        // Update last access for a specific tab with specific value
        useCases.updateLastAccess(tab.id, 123L)
        store.waitUntilIdle()
        assertEquals(123L, store.state.findTab(tab.id)?.lastAccess)

        // Update last access for currently selected tab
        store.dispatch(TabListAction.SelectTabAction(otherTab.id)).joinBlocking()
        assertEquals(0L, store.state.findTab(otherTab.id)?.lastAccess)
        useCases.updateLastAccess()
        store.waitUntilIdle()
        assertNotEquals(0L, store.state.findTab(otherTab.id)?.lastAccess)

        // Update last access for currently selected tab with specific value
        store.dispatch(TabListAction.SelectTabAction(otherTab.id)).joinBlocking()
        useCases.updateLastAccess(lastAccess = 345L)
        store.waitUntilIdle()
        assertEquals(345L, store.state.findTab(otherTab.id)?.lastAccess)
    }
}
