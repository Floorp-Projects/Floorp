/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.engine.middleware

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.selector.findCustomTab
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createCustomTab
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSessionState
import mozilla.components.support.test.any
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import mozilla.components.support.test.rule.runTestOnMain
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyBoolean
import org.mockito.ArgumentMatchers.anyString
import org.mockito.Mockito.never
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.mockito.Mockito.`when`

@RunWith(AndroidJUnit4::class)
class CreateEngineSessionMiddlewareTest {
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val dispatcher = coroutinesTestRule.testDispatcher
    private val scope = coroutinesTestRule.scope

    @Test
    fun `creates engine session if needed`() = runTestOnMain {
        val engine: Engine = mock()
        val engineSession: EngineSession = mock()
        whenever(engine.createSession(anyBoolean(), any())).thenReturn(engineSession)

        val middleware = CreateEngineSessionMiddleware(engine, scope)
        val tab = createTab("https://www.mozilla.org", id = "1")
        val store = BrowserStore(
            initialState = BrowserState(tabs = listOf(tab)),
            middleware = listOf(middleware),
        )
        assertNull(store.state.findTab(tab.id)?.engineState?.engineSession)

        store.dispatch(EngineAction.CreateEngineSessionAction(tab.id)).joinBlocking()
        store.waitUntilIdle()
        dispatcher.scheduler.advanceUntilIdle()
        verify(engine, times(1)).createSession(false)
        assertEquals(engineSession, store.state.findTab(tab.id)?.engineState?.engineSession)

        store.dispatch(EngineAction.CreateEngineSessionAction(tab.id)).joinBlocking()
        store.waitUntilIdle()
        dispatcher.scheduler.advanceUntilIdle()
        verify(engine, times(1)).createSession(false)
        assertEquals(engineSession, store.state.findTab(tab.id)?.engineState?.engineSession)
    }

    @Test
    fun `restores engine session state if available`() = runTestOnMain {
        val engine: Engine = mock()
        val engineSession: EngineSession = mock()
        whenever(engine.createSession(anyBoolean(), any())).thenReturn(engineSession)
        val engineSessionState: EngineSessionState = mock()

        val middleware = CreateEngineSessionMiddleware(engine, scope)
        val tab = createTab("https://www.mozilla.org", id = "1")
        val store = BrowserStore(
            initialState = BrowserState(tabs = listOf(tab)),
            middleware = listOf(middleware),
        )
        assertNull(store.state.findTab(tab.id)?.engineState?.engineSession)

        store.dispatch(EngineAction.UpdateEngineSessionStateAction(tab.id, engineSessionState)).joinBlocking()
        store.dispatch(EngineAction.CreateEngineSessionAction(tab.id)).joinBlocking()
        store.waitUntilIdle()
        dispatcher.scheduler.advanceUntilIdle()

        verify(engineSession).restoreState(engineSessionState)
        Unit
    }

    @Test
    fun `creates no engine session if tab does not exist`() = runTestOnMain {
        val engine: Engine = mock()
        `when`(engine.createSession(anyBoolean(), anyString())).thenReturn(mock())

        val middleware = CreateEngineSessionMiddleware(engine, scope)
        val store = BrowserStore(
            initialState = BrowserState(tabs = listOf()),
            middleware = listOf(middleware),
        )

        store.dispatch(EngineAction.CreateEngineSessionAction("invalid")).joinBlocking()
        store.waitUntilIdle()
        dispatcher.scheduler.advanceUntilIdle()

        verify(engine, never()).createSession(anyBoolean(), any())
        Unit
    }

    @Test
    fun `creates no engine session if session does not exist`() = runTestOnMain {
        val engine: Engine = mock()
        `when`(engine.createSession(anyBoolean(), anyString())).thenReturn(mock())

        val middleware = CreateEngineSessionMiddleware(engine, scope)
        val tab = createTab("https://www.mozilla.org", id = "1")

        val store = BrowserStore(
            initialState = BrowserState(tabs = listOf(tab)),
            middleware = listOf(middleware),
        )

        store.dispatch(
            EngineAction.CreateEngineSessionAction("non-existent"),
        ).joinBlocking()

        store.waitUntilIdle()
        dispatcher.scheduler.advanceUntilIdle()

        verify(engine, never()).createSession(anyBoolean(), any())
        Unit
    }

    @Test
    fun `dispatches follow-up action after engine session is created`() = runTestOnMain {
        val engine: Engine = mock()
        val engineSession: EngineSession = mock()
        whenever(engine.createSession(anyBoolean(), any())).thenReturn(engineSession)

        val middleware = CreateEngineSessionMiddleware(engine, scope)
        val tab = createTab("https://www.mozilla.org", id = "1")
        val store = BrowserStore(
            initialState = BrowserState(tabs = listOf(tab)),
            middleware = listOf(middleware),
        )
        assertNull(store.state.findTab(tab.id)?.engineState?.engineSession)

        val followupAction = ContentAction.UpdateTitleAction(tab.id, "test")
        store.dispatch(EngineAction.CreateEngineSessionAction(tab.id, followupAction = followupAction)).joinBlocking()

        store.waitUntilIdle()
        dispatcher.scheduler.advanceUntilIdle()

        verify(engine, times(1)).createSession(false)
        assertEquals(engineSession, store.state.findTab(tab.id)?.engineState?.engineSession)
        assertEquals(followupAction.title, store.state.findTab(tab.id)?.content?.title)
    }

    @Test
    fun `dispatches follow-up action once engine session is created by pending action`() = runTestOnMain {
        val engine: Engine = mock()
        val engineSession: EngineSession = mock()
        whenever(engine.createSession(anyBoolean(), any())).thenReturn(engineSession)

        val middleware = CreateEngineSessionMiddleware(engine, scope)
        val tab = createTab("https://www.mozilla.org", id = "1")
        val store = BrowserStore(
            initialState = BrowserState(tabs = listOf(tab)),
            middleware = listOf(middleware),
        )
        assertNull(store.state.findTab(tab.id)?.engineState?.engineSession)

        val followupAction = ContentAction.UpdateTitleAction(tab.id, "test")

        // Simulate two concurrent CreateEngineSessionActions
        store.dispatch(EngineAction.CreateEngineSessionAction(tab.id))
        store.dispatch(EngineAction.CreateEngineSessionAction(tab.id, followupAction = followupAction))

        store.waitUntilIdle()
        dispatcher.scheduler.advanceUntilIdle()

        verify(engine, times(1)).createSession(false)
        assertEquals(engineSession, store.state.findTab(tab.id)?.engineState?.engineSession)
        assertEquals(followupAction.title, store.state.findTab(tab.id)?.content?.title)
    }

    @Test
    fun `creating engine session for custom tab`() {
        val engine: Engine = mock()
        val engineSession: EngineSession = mock()
        whenever(engine.createSession(anyBoolean(), any())).thenReturn(engineSession)

        val middleware = CreateEngineSessionMiddleware(engine, scope)
        val customTab = createCustomTab("https://www.mozilla.org", id = "1")
        val store = BrowserStore(
            initialState = BrowserState(customTabs = listOf(customTab)),
            middleware = listOf(middleware),
        )
        assertNull(store.state.findCustomTab(customTab.id)?.engineState?.engineSession)

        val followupAction = ContentAction.UpdateTitleAction(customTab.id, "test")
        store.dispatch(EngineAction.CreateEngineSessionAction(customTab.id, followupAction = followupAction)).joinBlocking()

        store.waitUntilIdle()
        dispatcher.scheduler.advanceUntilIdle()

        verify(engine, times(1)).createSession(false)
        assertEquals(engineSession, store.state.findCustomTab(customTab.id)?.engineState?.engineSession)
        assertEquals(followupAction.title, store.state.findCustomTab(customTab.id)?.content?.title)
    }
}
