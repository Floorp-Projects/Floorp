/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.action

import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.engine.EngineSession
import mozilla.components.concept.engine.EngineSessionState
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test

class EngineActionTest {
    private lateinit var tab: TabSessionState
    private lateinit var store: BrowserStore

    @Before
    fun setUp() {
        tab = createTab("https://www.mozilla.org")

        store = BrowserStore(
            initialState = BrowserState(
                tabs = listOf(tab)
            )
        )
    }

    private fun engineState() = store.state.findTab(tab.id)!!.engineState

    @Test
    fun `LinkEngineSessionAction - Attaches engine session`() {
        assertNull(engineState().engineSession)

        val engineSession: EngineSession = mock()
        store.dispatch(EngineAction.LinkEngineSessionAction(tab.id, engineSession)).joinBlocking()

        assertNotNull(engineState().engineSession)
        assertEquals(engineSession, engineState().engineSession)
    }

    @Test
    fun `UnlinkEngineSessionAction - Detaches engine session`() {
        store.dispatch(EngineAction.LinkEngineSessionAction(tab.id, mock())).joinBlocking()
        assertNotNull(engineState().engineSession)

        store.dispatch(EngineAction.UnlinkEngineSessionAction(tab.id)).joinBlocking()
        assertNull(engineState().engineSession)
    }

    @Test
    fun `UpdateEngineSessionStateAction - Updates engine session state`() {
        assertNull(engineState().engineSessionState)

        val engineSessionState: EngineSessionState = mock()
        store.dispatch(EngineAction.UpdateEngineSessionStateAction(tab.id, engineSessionState)).joinBlocking()
        assertNotNull(engineState().engineSessionState)
        assertEquals(engineSessionState, engineState().engineSessionState)
    }
}
