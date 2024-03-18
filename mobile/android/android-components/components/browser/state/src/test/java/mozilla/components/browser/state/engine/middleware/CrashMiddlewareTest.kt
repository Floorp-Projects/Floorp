/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.engine.middleware

import mozilla.components.browser.state.action.CrashAction
import mozilla.components.browser.state.engine.EngineMiddleware
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.EngineState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.mockito.Mockito.doReturn

class CrashMiddlewareTest {
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val dispatcher = coroutinesTestRule.testDispatcher
    private val scope = coroutinesTestRule.scope

    @Test
    fun `Crash and restore scenario`() {
        val engineSession1: EngineSession = mock()
        val engineSession2: EngineSession = mock()
        val engineSession3: EngineSession = mock()

        val engine: Engine = mock()

        val store = BrowserStore(
            middleware = EngineMiddleware.create(
                engine = engine,
                scope = scope,
            ),
            initialState = BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "tab1").copy(
                        engineState = EngineState(engineSession1),
                    ),
                    createTab("https://www.firefox.com", id = "tab2").copy(
                        engineState = EngineState(engineSession2),
                    ),
                    createTab("https://getpocket.com", id = "tab3").copy(
                        engineState = EngineState(engineSession3),
                    ),
                ),
            ),
        )

        store.dispatch(
            CrashAction.SessionCrashedAction(
                "tab1",
            ),
        ).joinBlocking()

        store.dispatch(
            CrashAction.SessionCrashedAction(
                "tab3",
            ),
        ).joinBlocking()

        assertTrue(store.state.tabs[0].engineState.crashed)
        assertFalse(store.state.tabs[1].engineState.crashed)
        assertTrue(store.state.tabs[2].engineState.crashed)

        // Restoring crashed session
        store.dispatch(
            CrashAction.RestoreCrashedSessionAction(
                "tab1",
            ),
        ).joinBlocking()

        dispatcher.scheduler.advanceUntilIdle()

        assertFalse(store.state.tabs[0].engineState.crashed)
        assertFalse(store.state.tabs[1].engineState.crashed)
        assertTrue(store.state.tabs[2].engineState.crashed)

        // Restoring a non crashed session
        store.dispatch(
            CrashAction.RestoreCrashedSessionAction(
                "tab2",
            ),
        ).joinBlocking()

        dispatcher.scheduler.advanceUntilIdle()

        // Restoring unknown session
        store.dispatch(
            CrashAction.RestoreCrashedSessionAction(
                "unknown",
            ),
        ).joinBlocking()

        dispatcher.scheduler.advanceUntilIdle()

        assertFalse(store.state.tabs[0].engineState.crashed)
        assertFalse(store.state.tabs[1].engineState.crashed)
        assertTrue(store.state.tabs[2].engineState.crashed)
    }

    @Test
    fun `Restoring a crashed session without an engine session`() {
        val engineSession: EngineSession = mock()
        val engine: Engine = mock()
        doReturn(engineSession).`when`(engine).createSession()

        val store = BrowserStore(
            middleware = EngineMiddleware.create(
                engine = engine,
                scope = scope,
            ),
            initialState = BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "tab1"),
                ),
            ),
        )

        store.dispatch(
            CrashAction.SessionCrashedAction(
                "tab1",
            ),
        ).joinBlocking()

        dispatcher.scheduler.advanceUntilIdle()

        assertTrue(store.state.tabs[0].engineState.crashed)

        store.dispatch(
            CrashAction.RestoreCrashedSessionAction(
                "tab1",
            ),
        ).joinBlocking()

        dispatcher.scheduler.advanceUntilIdle()

        assertFalse(store.state.tabs[0].engineState.crashed)
    }
}
