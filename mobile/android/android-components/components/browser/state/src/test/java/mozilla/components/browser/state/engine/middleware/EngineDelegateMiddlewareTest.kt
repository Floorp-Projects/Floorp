/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.engine.middleware

import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.engine.EngineMiddleware
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.EngineState
import mozilla.components.browser.state.state.createCustomTab
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Rule
import org.junit.Test
import org.mockito.ArgumentMatchers
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

class EngineDelegateMiddlewareTest {
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val dispatcher = coroutinesTestRule.testDispatcher
    private val scope = coroutinesTestRule.scope

    @Test
    fun `LoadUrlAction for tab without engine session`() {
        val engineSession: EngineSession = mock()
        val engine: Engine = mock()
        doReturn(engineSession).`when`(engine).createSession()

        val tab = createTab("https://www.mozilla.org", id = "test-tab")
        val store = BrowserStore(
            middleware = EngineMiddleware.create(
                engine = engine,
                scope = scope,
            ),
            initialState = BrowserState(
                tabs = listOf(tab),
            ),
        )

        store.dispatch(
            EngineAction.LoadUrlAction(
                "test-tab",
                "https://www.firefox.com",
            ),
        ).joinBlocking()

        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()

        verify(engine).createSession(private = false, contextId = null)
        verify(engineSession, times(1)).loadUrl("https://www.firefox.com")
        assertEquals(engineSession, store.state.tabs[0].engineState.engineSession)
    }

    @Test
    fun `LoadUrlAction for private tab without engine session`() {
        val engineSession: EngineSession = mock()
        val engine: Engine = mock()
        doReturn(engineSession).`when`(engine).createSession(private = true)

        val tab = createTab("https://www.mozilla.org", id = "test-tab", private = true)
        val store = BrowserStore(
            middleware = EngineMiddleware.create(
                engine = engine,
                scope = scope,
            ),
            initialState = BrowserState(
                tabs = listOf(tab),
            ),
        )

        store.dispatch(
            EngineAction.LoadUrlAction(
                "test-tab",
                "https://www.firefox.com",
            ),
        ).joinBlocking()

        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()

        verify(engine).createSession(private = true, contextId = null)
        verify(engineSession, times(1)).loadUrl("https://www.firefox.com")
        assertEquals(engineSession, store.state.tabs[0].engineState.engineSession)
    }

    @Test
    fun `LoadUrlAction for container tab without engine session`() {
        val engineSession: EngineSession = mock()
        val engine: Engine = mock()
        doReturn(engineSession).`when`(engine).createSession(contextId = "test-container")

        val tab = createTab("https://www.mozilla.org", id = "test-tab", contextId = "test-container")
        val store = BrowserStore(
            middleware = EngineMiddleware.create(
                engine = engine,
                scope = scope,
            ),
            initialState = BrowserState(
                tabs = listOf(tab),
            ),
        )

        store.dispatch(
            EngineAction.LoadUrlAction(
                "test-tab",
                "https://www.firefox.com",
            ),
        ).joinBlocking()

        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()

        verify(engine).createSession(private = false, contextId = "test-container")
        verify(engineSession, times(1)).loadUrl("https://www.firefox.com")
        assertEquals(engineSession, store.state.tabs[0].engineState.engineSession)
    }

    @Test
    fun `LoadUrlAction for tab with engine session`() {
        val engineSession: EngineSession = mock()
        val engine: Engine = mock()

        val store = BrowserStore(
            middleware = EngineMiddleware.create(
                engine = engine,
                scope = scope,
            ),
            initialState = BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "test-tab").copy(
                        engineState = EngineState(engineSession),
                    ),
                ),
            ),
        )

        store.dispatch(
            EngineAction.LoadUrlAction(
                "test-tab",
                "https://www.firefox.com",
            ),
        ).joinBlocking()

        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()

        verify(engine, never()).createSession(ArgumentMatchers.anyBoolean(), ArgumentMatchers.anyString())
        verify(engineSession, times(1)).loadUrl("https://www.firefox.com")
        assertEquals(engineSession, store.state.tabs[0].engineState.engineSession)
    }

    @Test
    fun `LoadUrlAction for private tab with engine session`() {
        val engineSession: EngineSession = mock()
        val engine: Engine = mock()

        val store = BrowserStore(
            middleware = EngineMiddleware.create(
                engine = engine,
                scope = scope,
            ),
            initialState = BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "test-tab", private = true).copy(
                        engineState = EngineState(engineSession),
                    ),
                ),
            ),
        )

        store.dispatch(
            EngineAction.LoadUrlAction(
                "test-tab",
                "https://www.firefox.com",
            ),
        ).joinBlocking()

        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()

        verify(engine, never()).createSession(ArgumentMatchers.anyBoolean(), ArgumentMatchers.anyString())
        verify(engineSession, times(1)).loadUrl("https://www.firefox.com")
        assertEquals(engineSession, store.state.tabs[0].engineState.engineSession)
    }

    @Test
    fun `LoadUrlAction for container tab with engine session`() {
        val engineSession: EngineSession = mock()
        val engine: Engine = mock()

        val store = BrowserStore(
            middleware = EngineMiddleware.create(
                engine = engine,
                scope = scope,
            ),
            initialState = BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "test-tab", contextId = "test-container").copy(
                        engineState = EngineState(engineSession),
                    ),
                ),
            ),
        )

        store.dispatch(
            EngineAction.LoadUrlAction(
                "test-tab",
                "https://www.firefox.com",
            ),
        ).joinBlocking()

        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()

        verify(engine, never()).createSession(ArgumentMatchers.anyBoolean(), ArgumentMatchers.anyString())
        verify(engineSession, times(1)).loadUrl("https://www.firefox.com")
        assertEquals(engineSession, store.state.tabs[0].engineState.engineSession)
    }

    @Test
    fun `LoadUrlAction for tab with parent tab`() {
        val engineSession: EngineSession = mock()
        val engine: Engine = mock()
        doReturn(engineSession).`when`(engine).createSession()

        val parentEngineSession: EngineSession = mock()

        val parent = createTab("https://getpocket.com", id = "parent-tab").copy(
            engineState = EngineState(parentEngineSession),
        )
        val tab = createTab("https://www.mozilla.org", id = "test-tab", parent = parent)

        val store = BrowserStore(
            middleware = EngineMiddleware.create(
                engine = engine,
                scope = scope,
            ),
            initialState = BrowserState(
                tabs = listOf(parent, tab),
            ),
        )

        store.dispatch(
            EngineAction.LoadUrlAction(
                "test-tab",
                "https://www.firefox.com",
            ),
        ).joinBlocking()

        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()

        verify(engine).createSession(private = false, contextId = null)
        verify(engineSession, times(1)).loadUrl("https://www.firefox.com", parentEngineSession)
        assertEquals(parentEngineSession, store.state.tabs[0].engineState.engineSession)
        assertEquals(engineSession, store.state.tabs[1].engineState.engineSession)
    }

    @Test
    fun `LoadUrlAction for tab with parent tab without engine session`() {
        val engineSession: EngineSession = mock()
        val engine: Engine = mock()
        doReturn(engineSession).`when`(engine).createSession()

        val parent = createTab("https://getpocket.com", id = "parent-tab")
        val tab = createTab("https://www.mozilla.org", id = "test-tab", parent = parent)

        val store = BrowserStore(
            middleware = EngineMiddleware.create(
                engine = engine,
                scope = scope,
            ),
            initialState = BrowserState(
                tabs = listOf(parent, tab),
            ),
        )

        store.dispatch(
            EngineAction.LoadUrlAction(
                "test-tab",
                "https://www.firefox.com",
            ),
        ).joinBlocking()

        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()

        verify(engine, times(1)).createSession(private = false, contextId = null)
        verify(engineSession, times(1)).loadUrl("https://www.firefox.com")
        assertEquals(engineSession, store.state.tabs[1].engineState.engineSession)
    }

    @Test
    fun `LoadUrlAction with flags and additional headers`() {
        val engineSession: EngineSession = mock()

        val store = BrowserStore(
            middleware = EngineMiddleware.create(
                engine = mock(),
                scope = scope,
            ),
            initialState = BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "test-tab").copy(
                        engineState = EngineState(engineSession),
                    ),
                ),
            ),
        )

        store.dispatch(
            EngineAction.LoadUrlAction(
                "test-tab",
                "https://www.firefox.com",
                EngineSession.LoadUrlFlags.external(),
                mapOf(
                    "X-Coffee" to "Large",
                    "X-Sugar" to "None",
                ),
            ),
        ).joinBlocking()

        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()

        verify(engineSession, times(1)).loadUrl(
            "https://www.firefox.com",
            flags = EngineSession.LoadUrlFlags.external(),
            additionalHeaders = mapOf(
                "X-Coffee" to "Large",
                "X-Sugar" to "None",
            ),
        )
        assertEquals(engineSession, store.state.tabs[0].engineState.engineSession)
    }

    @Test
    fun `LoadUrlAction for tab with same url and without engine session`() {
        val engineSession: EngineSession = mock()
        val engine: Engine = mock()
        doReturn(engineSession).`when`(engine).createSession()

        val tab = createTab("https://www.mozilla.org", id = "test-tab")

        val store = BrowserStore(
            middleware = EngineMiddleware.create(
                engine = engine,
                scope = scope,
            ),
            initialState = BrowserState(
                tabs = listOf(tab),
            ),
        )

        store.dispatch(
            EngineAction.LoadUrlAction(
                "test-tab",
                "https://www.mozilla.org",
            ),
        ).joinBlocking()

        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()

        verify(engine).createSession(private = false, contextId = null)
        verify(engineSession, times(1)).loadUrl("https://www.mozilla.org")

        assertEquals(engineSession, store.state.tabs[0].engineState.engineSession)
    }

    @Test
    fun `LoadUrlAction for not existing tab`() {
        val engine: Engine = mock()

        val store = BrowserStore(
            middleware = EngineMiddleware.create(
                engine = engine,
                scope = scope,
            ),
            initialState = BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "test-tab"),
                ),
            ),
        )

        store.dispatch(
            EngineAction.LoadUrlAction(
                "unknown-tab",
                "https://www.mozilla.org",
            ),
        ).joinBlocking()

        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()

        verify(engine, never()).createSession(ArgumentMatchers.anyBoolean(), ArgumentMatchers.anyString())
        assertNull(store.state.tabs[0].engineState.engineSession)
    }

    @Test
    fun `LoadDataAction for tab without EngineSession`() {
        val engineSession: EngineSession = mock()
        val engine: Engine = mock()
        doReturn(engineSession).`when`(engine).createSession()

        val tab = createTab("https://www.mozilla.org", id = "test-tab")
        val store = BrowserStore(
            middleware = EngineMiddleware.create(
                engine = engine,
                scope = scope,
            ),
            initialState = BrowserState(
                tabs = listOf(tab),
            ),
        )

        store.dispatch(
            EngineAction.LoadDataAction(
                "test-tab",
                data = "foobar data",
                mimeType = "something/important",
                encoding = "UTF-16",
            ),
        ).joinBlocking()

        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()

        verify(engine).createSession(private = false, contextId = null)
        verify(engineSession, times(1)).loadData(
            data = "foobar data",
            mimeType = "something/important",
            encoding = "UTF-16",
        )
        assertEquals(engineSession, store.state.tabs[0].engineState.engineSession)
    }

    @Test
    fun `ReloadAction for tab without EngineSession`() {
        val engineSession: EngineSession = mock()
        val engine: Engine = mock()
        doReturn(engineSession).`when`(engine).createSession()

        val tab = createTab("https://www.mozilla.org", id = "test-tab")
        val store = BrowserStore(
            middleware = EngineMiddleware.create(
                engine = engine,
                scope = scope,
            ),
            initialState = BrowserState(
                tabs = listOf(tab),
            ),
        )

        store.dispatch(
            EngineAction.ReloadAction(
                "test-tab",
                flags = EngineSession.LoadUrlFlags.select(EngineSession.LoadUrlFlags.BYPASS_CACHE),
            ),
        ).joinBlocking()

        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()

        verify(engine).createSession(private = false, contextId = null)
        verify(engineSession, times(1)).reload(
            EngineSession.LoadUrlFlags.select(EngineSession.LoadUrlFlags.BYPASS_CACHE),
        )
        assertEquals(engineSession, store.state.tabs[0].engineState.engineSession)
    }

    @Test
    fun `GoForwardAction for tab without EngineSession`() {
        val engineSession: EngineSession = mock()
        val engine: Engine = mock()
        doReturn(engineSession).`when`(engine).createSession()

        val tab = createTab("https://www.mozilla.org", id = "test-tab")
        val store = BrowserStore(
            middleware = EngineMiddleware.create(
                engine = engine,
                scope = scope,
            ),
            initialState = BrowserState(
                tabs = listOf(tab),
            ),
        )

        store.dispatch(
            EngineAction.GoForwardAction(
                "test-tab",
            ),
        ).joinBlocking()

        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()

        verify(engine).createSession(private = false, contextId = null)
        verify(engineSession, times(1)).goForward()
        assertEquals(engineSession, store.state.tabs[0].engineState.engineSession)
    }

    @Test
    fun `GoBackAction for tab without EngineSession`() {
        val engineSession: EngineSession = mock()
        val engine: Engine = mock()
        doReturn(engineSession).`when`(engine).createSession()

        val tab = createTab("https://www.mozilla.org", id = "test-tab")
        val store = BrowserStore(
            middleware = EngineMiddleware.create(
                engine = engine,
                scope = scope,
            ),
            initialState = BrowserState(
                tabs = listOf(tab),
            ),
        )

        store.dispatch(
            EngineAction.GoBackAction(
                "test-tab",
            ),
        ).joinBlocking()

        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()

        verify(engine).createSession(private = false, contextId = null)
        verify(engineSession, times(1)).goBack()
        assertEquals(engineSession, store.state.tabs[0].engineState.engineSession)
    }

    @Test
    fun `GoToHistoryIndexAction for tab without EngineSession`() {
        val engineSession: EngineSession = mock()
        val engine: Engine = mock()
        doReturn(engineSession).`when`(engine).createSession()

        val tab = createTab("https://www.mozilla.org", id = "test-tab")
        val store = BrowserStore(
            middleware = EngineMiddleware.create(
                engine = engine,
                scope = scope,
            ),
            initialState = BrowserState(
                tabs = listOf(tab),
            ),
        )

        store.dispatch(
            EngineAction.GoToHistoryIndexAction(
                "test-tab",
                index = 42,
            ),
        ).joinBlocking()

        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()

        verify(engine).createSession(private = false, contextId = null)
        verify(engineSession, times(1)).goToHistoryIndex(42)
        assertEquals(engineSession, store.state.tabs[0].engineState.engineSession)
    }

    @Test
    fun `ToggleDesktopModeAction - Enable desktop mode`() {
        val engineSession: EngineSession = mock()
        val engine: Engine = mock()
        doReturn(engineSession).`when`(engine).createSession()

        val tab = createTab("https://www.mozilla.org", id = "test-tab")
        val store = BrowserStore(
            middleware = EngineMiddleware.create(
                engine = engine,
                scope = scope,
            ),
            initialState = BrowserState(
                tabs = listOf(tab),
            ),
        )

        store.dispatch(
            EngineAction.ToggleDesktopModeAction(
                "test-tab",
                enable = true,
            ),
        ).joinBlocking()

        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()

        verify(engine).createSession(private = false, contextId = null)
        verify(engineSession, times(1)).toggleDesktopMode(enable = true, reload = true)
        assertEquals(engineSession, store.state.tabs[0].engineState.engineSession)
    }

    @Test
    fun `ToggleDesktopModeAction - Disable desktop mode`() {
        val engineSession: EngineSession = mock()
        val engine: Engine = mock()
        doReturn(engineSession).`when`(engine).createSession()

        val tab = createTab("https://www.mozilla.org", id = "test-tab")
        val store = BrowserStore(
            middleware = EngineMiddleware.create(
                engine = engine,
                scope = scope,
            ),
            initialState = BrowserState(
                tabs = listOf(tab),
            ),
        )

        store.dispatch(
            EngineAction.ToggleDesktopModeAction(
                "test-tab",
                enable = false,
            ),
        ).joinBlocking()

        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()

        verify(engine).createSession(private = false, contextId = null)
        verify(engineSession, times(1)).toggleDesktopMode(enable = false, reload = true)
        assertEquals(engineSession, store.state.tabs[0].engineState.engineSession)
    }

    @Test
    fun `ExitFullscreenModeAction for tab without EngineSession`() {
        val engineSession: EngineSession = mock()
        val engine: Engine = mock()
        doReturn(engineSession).`when`(engine).createSession()

        val tab = createTab("https://www.mozilla.org", id = "test-tab")
        val store = BrowserStore(
            middleware = EngineMiddleware.create(
                engine = engine,
                scope = scope,
            ),
            initialState = BrowserState(
                tabs = listOf(tab),
            ),
        )

        store.dispatch(
            EngineAction.ExitFullScreenModeAction(
                "test-tab",
            ),
        ).joinBlocking()

        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()

        verify(engine).createSession(private = false, contextId = null)
        verify(engineSession, times(1)).exitFullScreenMode()
        assertEquals(engineSession, store.state.tabs[0].engineState.engineSession)
    }

    @Test
    fun `ClearDataAction for tab without EngineSession`() {
        val engineSession: EngineSession = mock()
        val engine: Engine = mock()
        doReturn(engineSession).`when`(engine).createSession()

        val tab = createTab("https://www.mozilla.org", id = "test-tab")
        val store = BrowserStore(
            middleware = EngineMiddleware.create(
                engine = engine,
                scope = scope,
            ),
            initialState = BrowserState(
                tabs = listOf(tab),
            ),
        )

        store.dispatch(
            EngineAction.ClearDataAction(
                "test-tab",
                data = Engine.BrowsingData.allCaches(),
            ),
        ).joinBlocking()

        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()

        verify(engine).createSession(private = false, contextId = null)
        verify(engineSession, times(1)).clearData(Engine.BrowsingData.allCaches())
        assertEquals(engineSession, store.state.tabs[0].engineState.engineSession)
    }

    @Test
    fun `PurgeHistoryAction - calls purgeHistory on engine session instances`() {
        val engineSession1: EngineSession = mock()
        val engineSession2: EngineSession = mock()

        val store = BrowserStore(
            middleware = EngineMiddleware.create(
                engine = mock(),
                scope = scope,
            ),
            initialState = BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org").copy(
                        engineState = EngineState(engineSession = null, engineSessionState = mock()),
                    ),
                    createTab("https://www.firefox.com").copy(
                        engineState = EngineState(engineSession = engineSession1, engineSessionState = mock()),
                    ),
                ),
                customTabs = listOf(
                    createCustomTab("http://www.theverge.com").copy(
                        engineState = EngineState(engineSession = null, engineSessionState = mock()),
                    ),
                    createCustomTab("https://www.google.com").copy(
                        engineState = EngineState(engineSession = engineSession2, engineSessionState = mock()),
                    ),
                ),
            ),
        )

        store.dispatch(EngineAction.PurgeHistoryAction).joinBlocking()

        dispatcher.scheduler.advanceUntilIdle()

        verify(engineSession1).purgeHistory()
        verify(engineSession2).purgeHistory()
    }
}
