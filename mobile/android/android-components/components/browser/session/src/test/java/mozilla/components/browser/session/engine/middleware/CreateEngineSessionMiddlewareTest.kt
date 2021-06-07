/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.engine.middleware

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.test.TestCoroutineDispatcher
import mozilla.components.browser.session.Session
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSessionState
import mozilla.components.support.test.any
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.whenever
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.anyBoolean
import org.mockito.Mockito.never
import org.mockito.Mockito.times
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class CreateEngineSessionMiddlewareTest {

    private val dispatcher = TestCoroutineDispatcher()
    private val scope = CoroutineScope(dispatcher)

    @After
    fun tearDown() {
        dispatcher.cleanupTestCoroutines()
    }

    @Test
    fun `creates engine session if needed`() = runBlocking {
        val engine: Engine = mock()
        val engineSession: EngineSession = mock()
        whenever(engine.createSession(anyBoolean(), any())).thenReturn(engineSession)

        val session: Session = mock()
        val sessionLookup = { _: String -> session }

        val middleware = CreateEngineSessionMiddleware(engine, sessionLookup, scope)
        val tab = createTab("https://www.mozilla.org", id = "1")
        val store = BrowserStore(
            initialState = BrowserState(tabs = listOf(tab)),
            middleware = listOf(middleware)
        )
        assertNull(store.state.findTab(tab.id)?.engineState?.engineSession)

        store.dispatch(EngineAction.CreateEngineSessionAction(tab.id)).joinBlocking()
        store.waitUntilIdle()
        dispatcher.advanceUntilIdle()
        verify(engine, times(1)).createSession(false)
        assertEquals(engineSession, store.state.findTab(tab.id)?.engineState?.engineSession)

        store.dispatch(EngineAction.CreateEngineSessionAction(tab.id)).joinBlocking()
        store.waitUntilIdle()
        dispatcher.advanceUntilIdle()
        verify(engine, times(1)).createSession(false)
        assertEquals(engineSession, store.state.findTab(tab.id)?.engineState?.engineSession)
    }

    @Test
    fun `restores engine session state if available`() = runBlocking {
        val engine: Engine = mock()
        val engineSession: EngineSession = mock()
        whenever(engine.createSession(anyBoolean(), any())).thenReturn(engineSession)
        val engineSessionState: EngineSessionState = mock()
        val session: Session = mock()
        val sessionLookup = { _: String -> session }

        val middleware = CreateEngineSessionMiddleware(engine, sessionLookup, scope)
        val tab = createTab("https://www.mozilla.org", id = "1")
        val store = BrowserStore(
            initialState = BrowserState(tabs = listOf(tab)),
            middleware = listOf(middleware)
        )
        assertNull(store.state.findTab(tab.id)?.engineState?.engineSession)

        store.dispatch(EngineAction.UpdateEngineSessionStateAction(tab.id, engineSessionState)).joinBlocking()
        store.dispatch(EngineAction.CreateEngineSessionAction(tab.id)).joinBlocking()
        store.waitUntilIdle()
        dispatcher.advanceUntilIdle()

        verify(engineSession).restoreState(engineSessionState)
        Unit
    }

    @Test
    fun `creates no engine session if tab does not exist`() = runBlocking {
        val engine: Engine = mock()
        val session: Session = mock()
        val sessionLookup = { _: String -> session }

        val middleware = CreateEngineSessionMiddleware(engine, sessionLookup, scope)
        val store = BrowserStore(
            initialState = BrowserState(tabs = listOf()),
            middleware = listOf(middleware)
        )

        store.dispatch(EngineAction.CreateEngineSessionAction("invalid")).joinBlocking()
        store.waitUntilIdle()
        dispatcher.advanceUntilIdle()

        verify(engine, never()).createSession(anyBoolean(), any())
        Unit
    }

    @Test
    fun `creates no engine session if session does not exist`() = runBlocking {
        val engine: Engine = mock()
        val sessionLookup = { _: String -> null }

        val middleware = CreateEngineSessionMiddleware(engine, sessionLookup, scope)
        val tab = createTab("https://www.mozilla.org", id = "1")
        val store = BrowserStore(
            initialState = BrowserState(tabs = listOf(tab)),
            middleware = listOf(middleware)
        )

        store.dispatch(EngineAction.CreateEngineSessionAction(tab.id)).joinBlocking()
        store.waitUntilIdle()
        dispatcher.advanceUntilIdle()

        verify(engine, never()).createSession(anyBoolean(), any())
        Unit
    }
}
