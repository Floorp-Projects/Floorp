/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.action

import kotlinx.coroutines.runBlocking
import mozilla.components.browser.session.state.BrowserState
import mozilla.components.browser.session.state.SessionState
import mozilla.components.browser.session.store.BrowserStore
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class SessionListActionTest {
    @Test
    fun `AddSessionAction - Adds provided SessionState`() = runBlocking {
        val state = BrowserState()
        val store = BrowserStore(state)

        assertEquals(0, store.state.sessions.size)
        assertNull(store.state.selectedSessionId)

        val session = SessionState(url = "https://www.mozilla.org")

        store.dispatch(SessionListAction.AddSessionAction(session))
            .join()

        assertEquals(1, store.state.sessions.size)
        assertNull(store.state.selectedSessionId)
    }

    @Test
    fun `SelectSessionAction - Selects SessionState by id`() = runBlocking {
        val state = BrowserState(
            sessions = listOf(
                SessionState(id = "a", url = "https://www.mozilla.org"),
                SessionState(id = "b", url = "https://www.firefox.com")
            )
        )
        val store = BrowserStore(state)

        assertNull(store.state.selectedSessionId)

        store.dispatch(SessionListAction.SelectSessionAction("a"))
            .join()

        assertEquals("a", store.state.selectedSessionId)
    }

    @Test
    fun `RemoveSessionAction - Removes SessionState`() = runBlocking {
        val state = BrowserState(
            sessions = listOf(
                SessionState(id = "a", url = "https://www.mozilla.org"),
                SessionState(id = "b", url = "https://www.firefox.com")
            )
        )
        val store = BrowserStore(state)

        store.dispatch(SessionListAction.RemoveSessionAction("a"))
            .join()

        assertEquals(1, store.state.sessions.size)
        assertEquals("https://www.firefox.com", store.state.sessions[0].url)
    }

    @Test
    fun `RemoveSessionAction - Noop for unknown id`() = runBlocking {
        val state = BrowserState(
            sessions = listOf(
                SessionState(id = "a", url = "https://www.mozilla.org"),
                SessionState(id = "b", url = "https://www.firefox.com")
            )
        )
        val store = BrowserStore(state)

        store.dispatch(SessionListAction.RemoveSessionAction("c"))
            .join()

        assertEquals(2, store.state.sessions.size)
        assertEquals("https://www.mozilla.org", store.state.sessions[0].url)
        assertEquals("https://www.firefox.com", store.state.sessions[1].url)
    }
}