/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.engine.middleware

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.test.TestCoroutineDispatcher
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSessionState
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.mock
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class SuspendMiddlewareTest {

    private val dispatcher = TestCoroutineDispatcher()
    private val scope = CoroutineScope(dispatcher)

    @After
    fun tearDown() {
        dispatcher.cleanupTestCoroutines()
    }

    @Test
    fun `suspends engine session`() = runBlocking {
        val middleware = SuspendMiddleware(scope)

        val tab = createTab("https://www.mozilla.org", id = "1")
        val store = BrowserStore(
            initialState = BrowserState(tabs = listOf(tab)),
            middleware = listOf(middleware)
        )

        val engineSession: EngineSession = mock()
        store.dispatch(EngineAction.LinkEngineSessionAction(tab.id, engineSession)).joinBlocking()

        val state: EngineSessionState = mock()
        store.dispatch(EngineAction.UpdateEngineSessionStateAction(tab.id, state)).joinBlocking()

        store.dispatch(EngineAction.SuspendEngineSessionAction(tab.id)).joinBlocking()

        store.waitUntilIdle()
        dispatcher.advanceUntilIdle()

        assertNull(store.state.findTab(tab.id)?.engineState?.engineSession)
        assertEquals(state, store.state.findTab(tab.id)?.engineState?.engineSessionState)
        verify(engineSession).close()
    }

    @Test
    fun `does nothing if tab doesn't exist`() {
        val middleware = SuspendMiddleware(scope)

        val store = spy(BrowserStore(
            initialState = BrowserState(tabs = listOf()),
            middleware = listOf(middleware)
        ))

        store.dispatch(EngineAction.SuspendEngineSessionAction("invalid")).joinBlocking()
        verify(store, never()).dispatch(EngineAction.UnlinkEngineSessionAction("invalid"))
    }

    @Test
    fun `does nothing if engine session doesn't exist`() {
        val middleware = SuspendMiddleware(scope)

        val tab = createTab("https://www.mozilla.org", id = "1")
        val store = spy(BrowserStore(
            initialState = BrowserState(tabs = listOf(tab)),
            middleware = listOf(middleware)
        ))

        store.dispatch(EngineAction.SuspendEngineSessionAction(tab.id)).joinBlocking()
        verify(store, never()).dispatch(EngineAction.UnlinkEngineSessionAction(tab.id))
    }
}
