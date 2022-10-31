/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.search.middleware

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.LoadRequestState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.search.telemetry.ads.AdsTelemetry
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify

@RunWith(AndroidJUnit4::class)
class AdsTelemetryMiddlewareTest {
    val sessionId = "session"
    lateinit var adsMiddleware: AdsTelemetryMiddleware
    lateinit var browserState: BrowserState

    @Before
    fun setup() {
        adsMiddleware = AdsTelemetryMiddleware(mock())
        browserState = BrowserState(
            tabs = listOf(TabSessionState(content = ContentState("https://mozilla.org"), id = sessionId)),
        )
    }

    @Test
    fun `GIVEN redirectChain empty WHEN a new URL loads THEN the redirectChain starts from the current tab url`() {
        val store = BrowserStore(
            initialState = browserState,
            middleware = listOf(adsMiddleware),
        )

        store.dispatch(
            ContentAction.UpdateLoadRequestAction(
                sessionId,
                LoadRequestState(
                    url = "https://mozilla.org/firefox",
                    triggeredByRedirect = false,
                    triggeredByUser = false,
                ),
            ),
        ).joinBlocking()

        assertEquals(1, adsMiddleware.redirectChain.size)
        assertEquals("https://mozilla.org", adsMiddleware.redirectChain[sessionId]!!.root)
    }

    @Test
    fun `GIVEN redirectChain is not empty WHEN a new URL loads THEN that URL is added to the chain`() {
        adsMiddleware.redirectChain[sessionId] = RedirectChain("https://mozilla.org")
        val store = BrowserStore(
            initialState = browserState,
            middleware = listOf(adsMiddleware),
        )

        store.dispatch(
            ContentAction.UpdateLoadRequestAction(
                sessionId,
                LoadRequestState(
                    url = "https://mozilla.org/firefox",
                    triggeredByRedirect = false,
                    triggeredByUser = false,
                ),
            ),
        ).joinBlocking()

        assertEquals(1, adsMiddleware.redirectChain.size)
        assertEquals("https://mozilla.org", adsMiddleware.redirectChain[sessionId]!!.root)
        assertEquals(1, adsMiddleware.redirectChain.size)
        assertEquals("https://mozilla.org/firefox", adsMiddleware.redirectChain[sessionId]!!.chain[0])
    }

    @Test
    fun `WHEN the session URL is updated THEN check if an ad was clicked`() {
        val adsTelemetry: AdsTelemetry = mock()
        val adsMiddleware = AdsTelemetryMiddleware(adsTelemetry)
        adsMiddleware.redirectChain[sessionId] = RedirectChain("https://mozilla.org")
        adsMiddleware.redirectChain[sessionId]!!.chain.add("https://mozilla.org/firefox")
        val store = BrowserStore(
            initialState = browserState,
            middleware = listOf(adsMiddleware),
        )

        store
            .dispatch(ContentAction.UpdateUrlAction(sessionId, "https://mozilla.org/firefox"))
            .joinBlocking()

        verify(adsTelemetry).checkIfAddWasClicked(
            "https://mozilla.org",
            listOf("https://mozilla.org/firefox"),
        )
    }

    @Test
    fun `GIVEN a location update WHEN ads telemetry is recorded THEN redirect chain is reset`() {
        val tab = createTab(id = "1", url = "http://mozilla.org")
        val store = BrowserStore(
            initialState = browserState,
            middleware = listOf(adsMiddleware),
        )
        store.dispatch(TabListAction.AddTabAction(tab)).joinBlocking()
        store.dispatch(
            ContentAction.UpdateLoadRequestAction(
                tab.id,
                LoadRequestState("https://mozilla.org", true, true),
            ),
        ).joinBlocking()

        assertNotNull(adsMiddleware.redirectChain[tab.id])

        store.dispatch(ContentAction.UpdateUrlAction(tab.id, "https://mozilla.org")).joinBlocking()
        assertNull(adsMiddleware.redirectChain[tab.id])
    }
}
