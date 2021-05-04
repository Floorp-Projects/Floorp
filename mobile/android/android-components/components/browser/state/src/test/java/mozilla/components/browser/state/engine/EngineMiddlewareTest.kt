/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.engine

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.cancel
import kotlinx.coroutines.test.TestCoroutineDispatcher
import kotlinx.coroutines.test.resetMain
import kotlinx.coroutines.test.setMain
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.Engine
import mozilla.components.concept.engine.EngineSession
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import org.junit.After
import org.junit.Before
import org.junit.Test
import org.mockito.Mockito
import org.mockito.Mockito.verify

class EngineMiddlewareTest {
    private lateinit var dispatcher: TestCoroutineDispatcher
    private lateinit var scope: CoroutineScope

    @Before
    fun setUp() {
        dispatcher = TestCoroutineDispatcher()
        scope = CoroutineScope(dispatcher)

        Dispatchers.setMain(dispatcher)
    }

    @After
    fun tearDown() {
        dispatcher.cleanupTestCoroutines()
        scope.cancel()

        Dispatchers.resetMain()
    }

    @Test
    fun `Dispatching CreateEngineSessionAction multiple times should only create one engine session`() {
        val session: EngineSession = mock()
        val engine: Engine = mock()
        Mockito.doReturn(session).`when`(engine).createSession(false, null)

        val store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(
                    createTab("https://www.mozilla.org", id = "mozilla")
                )
            ),
            middleware = EngineMiddleware.create(engine, scope)
        )

        store.dispatch(
            EngineAction.CreateEngineSessionAction("mozilla")
        )

        store.dispatch(
            EngineAction.CreateEngineSessionAction("mozilla")
        )

        dispatcher.advanceUntilIdle()
        store.waitUntilIdle()

        verify(engine, Mockito.times(1)).createSession(false, null)
    }
}
