/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import mozilla.components.browser.state.action.LastAccessAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Test

class LastAccessReducerTest {
    @Test
    fun `WHEN the reducer is called for UpdateLastAccessAction THEN a new state with updated lastAccess is returned`() {
        val tab1 = TabSessionState(id = "tab1", lastAccess = 111, content = mock())
        val tab2 = TabSessionState(id = "tab2", lastAccess = 222, content = mock())
        val browserState = BrowserState(tabs = listOf(tab1, tab2))

        val updatedState = LastAccessReducer.reduce(
            browserState, LastAccessAction.UpdateLastAccessAction(tabId = "tab1", lastAccess = 345)
        )

        assertEquals(2, updatedState.tabs.size)
        assertEquals(345, updatedState.tabs[0].lastAccess)
        assertEquals(222, updatedState.tabs[1].lastAccess)
    }

    @Test
    fun `WHEN the reducer is called for UpdateLastMediaAccessAction THEN a new state with updated lastMediaAccess is returned`() {
        val tab1 = TabSessionState(id = "tab1", lastMediaAccess = 111, content = mock())
        val tab2 = TabSessionState(id = "tab2", lastMediaAccess = 222, content = mock())
        val browserState = BrowserState(tabs = listOf(tab1, tab2))

        val updatedState = LastAccessReducer.reduce(
            browserState, LastAccessAction.UpdateLastMediaAccessAction(tabId = "tab1", lastMediaAccess = 345)
        )

        assertEquals(2, updatedState.tabs.size)
        assertEquals(345, updatedState.tabs[0].lastMediaAccess)
        assertEquals(222, updatedState.tabs[1].lastMediaAccess)
    }
}
