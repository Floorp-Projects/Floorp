/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import mozilla.components.browser.state.action.ShareInternetResourceAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.content.ShareInternetResourceState
import mozilla.components.concept.fetch.Response
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class ShareInternetResourceStateReducerTest {

    @Test
    fun `reduce - AddShareAction should add the internetResource in the ContentState`() {
        val reducer = ShareInternetResourceStateReducer
        val state = BrowserState(tabs = listOf(TabSessionState("tabId", ContentState("contentStateUrl"))))
        val response: Response = mock()
        val action = ShareInternetResourceAction.AddShareAction(
            "tabId",
            ShareInternetResourceState("internetResourceUrl", "type", true, response),
        )

        assertNull(state.tabs[0].content.share)

        val result = reducer.reduce(state, action)

        val shareState = result.tabs[0].content.share!!
        assertEquals("internetResourceUrl", shareState.url)
        assertEquals("type", shareState.contentType)
        assertTrue(shareState.private)
        assertEquals(response, shareState.response)
    }

    @Test
    fun `reduce - ConsumeShareAction should remove the ShareInternetResourceState ContentState`() {
        val reducer = ShareInternetResourceStateReducer
        val shareState: ShareInternetResourceState = mock()
        val state = BrowserState(
            tabs = listOf(
                TabSessionState("tabId", ContentState("contentStateUrl", share = shareState)),
            ),
        )
        val action = ShareInternetResourceAction.ConsumeShareAction("tabId")

        assertNotNull(state.tabs[0].content.share)

        val result = reducer.reduce(state, action)

        assertNull(result.tabs[0].content.share)
    }

    @Test
    fun `updateTheContentState will return a new BrowserState with updated ContentState`() {
        val initialContentState = ContentState("emptyStateUrl")
        val browserState = BrowserState(tabs = listOf(TabSessionState("tabId", initialContentState)))

        val result = updateTheContentState(browserState, "tabId") { it.copy(url = "updatedUrl") }

        assertFalse(browserState == result)
        assertEquals("updatedUrl", result.tabs[0].content.url)
    }
}
