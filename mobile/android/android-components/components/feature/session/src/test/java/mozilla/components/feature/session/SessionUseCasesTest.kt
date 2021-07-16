/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.session

import kotlinx.coroutines.runBlocking
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.CrashAction
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.engine.EngineMiddleware
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

    @Before
    fun setUp() {
        engineSession = mock()
        middleware = CaptureActionsMiddleware()
        store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(
                    createTab(
                        url = "https://wwww.mozilla.org",
                        id = "mozilla",
                        engineSession = engineSession
                    )
                ),
                selectedTabId = "mozilla"
            ),
            middleware = listOf(middleware) + EngineMiddleware.create(engine = mock())
        )
        useCases = SessionUseCases(store)
    }

    @Test
    fun loadUrl() {
        useCases.loadUrl("https://getpocket.com")
        store.waitUntilIdle()

        middleware.assertLastAction(EngineAction.LoadUrlAction::class) { action ->
            assertEquals("mozilla", action.tabId)
            assertEquals("https://getpocket.com", action.url)
        }

        useCases.loadUrl("http://www.mozilla.org", LoadUrlFlags.select(LoadUrlFlags.EXTERNAL))
        store.waitUntilIdle()

        middleware.assertLastAction(EngineAction.LoadUrlAction::class) { action ->
            assertEquals("mozilla", action.tabId)
            assertEquals("http://www.mozilla.org", action.url)
            assertEquals(LoadUrlFlags.select(LoadUrlFlags.EXTERNAL), action.flags)
        }

        useCases.loadUrl("http://getpocket.com", store.state.selectedTabId)
        store.waitUntilIdle()

        middleware.assertLastAction(EngineAction.LoadUrlAction::class) { action ->
            assertEquals("mozilla", action.tabId)
            assertEquals("http://getpocket.com", action.url)
        }

        useCases.loadUrl.invoke(
            "http://getpocket.com",
            store.state.selectedTabId,
            LoadUrlFlags.select(LoadUrlFlags.BYPASS_PROXY)
        )
        store.waitUntilIdle()

        middleware.assertLastAction(EngineAction.LoadUrlAction::class) { action ->
            assertEquals("mozilla", action.tabId)
            assertEquals("http://getpocket.com", action.url)
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
            tabId = store.state.selectedTabId
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
            store.state.selectedTabId
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
    fun stopLoading() = runBlocking {
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
        }
        middleware.reset()

        useCases.goBack()
        store.waitUntilIdle()
        middleware.assertLastAction(EngineAction.GoBackAction::class) { action ->
            assertEquals("mozilla", action.tabId)
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
        }
        middleware.reset()

        useCases.goForward()
        store.waitUntilIdle()
        middleware.assertLastAction(EngineAction.GoForwardAction::class) { action ->
            assertEquals("mozilla", action.tabId)
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
        val store = spy(BrowserStore(
            middleware = listOf(middleware),
            initialState = BrowserState(
                tabs = listOf(
                    createTab(url = "https://wwww.mozilla.org", id = "tab1", crashed = true)
                ),
                customTabs = listOf(
                    createCustomTab("https://wwww.mozilla.org", id = "customTab1",
                        crashed = true)
                    )
                )
            )
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
}
