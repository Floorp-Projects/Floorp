/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import mozilla.components.browser.state.action.CopyInternetResourceAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.content.ShareInternetResourceState
import mozilla.components.concept.fetch.Response
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class CopyInternetResourceStateReducerTest {
    @Test
    fun `reduce - AddCopyAction should add the internetResource in the ContentState`() {
        val reducer = CopyInternetResourceStateReducer
        val state = BrowserState(tabs = listOf(TabSessionState("tabId", ContentState("contentStateUrl"))))
        val response: Response = mock()
        val action = CopyInternetResourceAction.AddCopyAction(
            "tabId",
            ShareInternetResourceState("internetResourceUrl", "type", true, response),
        )

        assertNull(state.tabs[0].content.copy)

        val result = reducer.reduce(state, action)

        val copyState = result.tabs[0].content.copy!!
        assertEquals("internetResourceUrl", copyState.url)
        assertEquals("type", copyState.contentType)
        assertTrue(copyState.private)
        assertEquals(response, copyState.response)
    }

    @Test
    fun `reduce - ConsumeCopyAction should remove the CopyInternetResourceState ContentState`() {
        val reducer = CopyInternetResourceStateReducer
        val shareState: ShareInternetResourceState = mock()
        val state = BrowserState(
            tabs = listOf(
                TabSessionState("tabId", ContentState("contentStateUrl", copy = shareState)),
            ),
        )
        val action = CopyInternetResourceAction.ConsumeCopyAction("tabId")

        assertNotNull(state.tabs[0].content.copy)

        val result = reducer.reduce(state, action)

        assertNull(result.tabs[0].content.copy)
    }
}
