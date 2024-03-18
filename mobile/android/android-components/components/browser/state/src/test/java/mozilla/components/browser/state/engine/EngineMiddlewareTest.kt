/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.engine

import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.engine.middleware.TrimMemoryMiddleware
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.mockito.Mockito
import org.mockito.Mockito.verify

class EngineMiddlewareTest {
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val dispatcher = coroutinesTestRule.testDispatcher
    private val scope = coroutinesTestRule.scope

    @Test
    fun `Dispatching CreateEngineSessionAction multiple times should only create one engine session`() {
        val session: EngineSession = mock()
        val engine: Engine = mock()
        Mockito.doReturn(session).`when`(engine).createSession(false, null)

        val store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "mozilla"),
                ),
            ),
            middleware = EngineMiddleware.create(engine, scope),
        )

        store.dispatch(
            EngineAction.CreateEngineSessionAction("mozilla"),
        )

        store.dispatch(
            EngineAction.CreateEngineSessionAction("mozilla"),
        )

        dispatcher.scheduler.advanceUntilIdle()
        store.waitUntilIdle()

        verify(engine, Mockito.times(1)).createSession(false, null)
    }

    @Test
    fun `TrimMemoryMiddleware will be added by default`() {
        val list = EngineMiddleware.create(
            engine = mock(),
        )

        assertTrue(list.any { it is TrimMemoryMiddleware })
    }

    @Test
    fun `TrimMemoryMiddleware will not be added if trimMemoryAutomatically is set to false`() {
        val list = EngineMiddleware.create(
            engine = mock(),
            trimMemoryAutomatically = false,
        )

        assertTrue(list.none { it is TrimMemoryMiddleware })
    }
}
