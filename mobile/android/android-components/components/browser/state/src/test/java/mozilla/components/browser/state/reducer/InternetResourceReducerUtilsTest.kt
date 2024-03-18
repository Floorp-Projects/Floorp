/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.TabSessionState
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Test

class InternetResourceReducerUtilsTest {
    @Test
    fun `updateTheContentState will return a new BrowserState with updated ContentState`() {
        val initialContentState = ContentState("emptyStateUrl")
        val browserState =
            BrowserState(tabs = listOf(TabSessionState("tabId", initialContentState)))

        val result = updateTheContentState(browserState, "tabId") { it.copy(url = "updatedUrl") }

        assertFalse(browserState == result)
        assertEquals("updatedUrl", result.tabs[0].content.url)
    }
}
