/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.engine.middleware

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.test.TestCoroutineDispatcher
import mozilla.components.browser.session.Session
import mozilla.components.browser.session.engine.EngineMiddleware
import mozilla.components.browser.state.action.CrashAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.EngineState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.never
import org.mockito.Mockito.reset
import org.mockito.Mockito.verify

class CrashMiddlewareTest {
    @Test
    fun `Crash and restore scenario`() {
        val engineSession1: EngineSession = mock()
        val engineSession2: EngineSession = mock()
        val engineSession3: EngineSession = mock()

        val engine: Engine = mock()
        val dispatcher = TestCoroutineDispatcher()
        val scope = CoroutineScope(dispatcher)

        val store = BrowserStore(
            middleware = EngineMiddleware.create(
                engine = engine,
                sessionLookup = { mock() },
                scope = scope
            ),
            initialState = BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "tab1").copy(
                        engineState = EngineState(engineSession1)
                    ),
                    createTab("https://www.firefox.com", id = "tab2").copy(
                        engineState = EngineState(engineSession2)
                    ),
                    createTab("https://getpocket.com", id = "tab3").copy(
                        engineState = EngineState(engineSession3)
                    )
                )
            )
        )

        store.dispatch(CrashAction.SessionCrashedAction(
            "tab1"
        )).joinBlocking()

        store.dispatch(CrashAction.SessionCrashedAction(
            "tab3"
        )).joinBlocking()

        assertTrue(store.state.tabs[0].crashed)
        assertFalse(store.state.tabs[1].crashed)
        assertTrue(store.state.tabs[2].crashed)

        verify(engineSession1, never()).recoverFromCrash()
        verify(engineSession2, never()).recoverFromCrash()
        verify(engineSession3, never()).recoverFromCrash()

        // Restoring crashed session
        store.dispatch(CrashAction.RestoreCrashedSessionAction(
            "tab1"
        )).joinBlocking()

        dispatcher.advanceUntilIdle()

        verify(engineSession1).recoverFromCrash()
        verify(engineSession2, never()).recoverFromCrash()
        verify(engineSession3, never()).recoverFromCrash()
        reset(engineSession1, engineSession2, engineSession3)

        assertFalse(store.state.tabs[0].crashed)
        assertFalse(store.state.tabs[1].crashed)
        assertTrue(store.state.tabs[2].crashed)

        // Restoring a non crashed session
        store.dispatch(CrashAction.RestoreCrashedSessionAction(
            "tab2"
        )).joinBlocking()

        dispatcher.advanceUntilIdle()

        // EngineSession.recoverFromCrash() handles internally the situation where there's no
        // crashed state.
        verify(engineSession1, never()).recoverFromCrash()
        verify(engineSession2).recoverFromCrash()
        verify(engineSession3, never()).recoverFromCrash()
        reset(engineSession1, engineSession2, engineSession3)

        // Restoring unknown session
        store.dispatch(CrashAction.RestoreCrashedSessionAction(
            "unknown"
        )).joinBlocking()

        dispatcher.advanceUntilIdle()

        verify(engineSession1, never()).recoverFromCrash()
        verify(engineSession2, never()).recoverFromCrash()
        verify(engineSession3, never()).recoverFromCrash()

        assertFalse(store.state.tabs[0].crashed)
        assertFalse(store.state.tabs[1].crashed)
        assertTrue(store.state.tabs[2].crashed)
    }

    @Test
    fun `Restoring a crashed session without an engine session`() {
        val engineSession: EngineSession = mock()
        val engine: Engine = mock()
        doReturn(engineSession).`when`(engine).createSession()

        val dispatcher = TestCoroutineDispatcher()
        val scope = CoroutineScope(dispatcher)

        val store = BrowserStore(
            middleware = EngineMiddleware.create(
                engine = engine,
                sessionLookup = { Session("https://www.mozilla.org") },
                scope = scope
            ),
            initialState = BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "tab1")
                )
            )
        )

        store.dispatch(CrashAction.SessionCrashedAction(
            "tab1"
        )).joinBlocking()

        dispatcher.advanceUntilIdle()

        assertTrue(store.state.tabs[0].crashed)

        store.dispatch(CrashAction.RestoreCrashedSessionAction(
            "tab1"
        )).joinBlocking()

        dispatcher.advanceUntilIdle()

        verify(engine).createSession()
        verify(engineSession).recoverFromCrash()

        assertFalse(store.state.tabs[0].crashed)
    }
}
