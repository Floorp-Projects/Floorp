/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import mozilla.components.browser.state.action.DebugAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.lib.state.DelicateAction
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Test

@DelicateAction
class DebugReducerTest {
    @Test
    fun `WHEN the reducer is called for UpdateCreatedAtAction THEN a new state with updated createdAt is returned`() {
        val tab1 = TabSessionState(id = "tab1", content = mock())
        val tab2 = TabSessionState(id = "tab2", content = mock())
        val browserState = BrowserState(tabs = listOf(tab1, tab2))

        val updatedState = DebugReducer.reduce(
            browserState, DebugAction.UpdateCreatedAtAction(tabId = "tab1", createdAt = 345L)
        )

        assertEquals(2, updatedState.tabs.size)
        assertEquals(345, updatedState.tabs[0].createdAt)
        assertNotEquals(345, updatedState.tabs[1].createdAt)
    }
}
