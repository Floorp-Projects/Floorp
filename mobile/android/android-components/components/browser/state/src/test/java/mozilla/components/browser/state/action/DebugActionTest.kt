/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.action

import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.DelicateAction
import mozilla.components.support.test.ext.joinBlocking
import org.junit.Assert.assertEquals
import org.junit.Test

@DelicateAction
class DebugActionTest {
    @Test
    fun `UpdateCreatedAtAction - updates createdAt when the tab was first created`() {
        val existingTab = createTab("https://www.mozilla.org")

        val state = BrowserState(
            tabs = listOf(existingTab),
            selectedTabId = existingTab.id
        )

        val store = BrowserStore(state)
        val timestamp = System.currentTimeMillis()

        store.dispatch(DebugAction.UpdateCreatedAtAction(existingTab.id, timestamp)).joinBlocking()

        assertEquals(timestamp, store.state.selectedTab?.createdAt)
    }
}
