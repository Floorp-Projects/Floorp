/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session

import mozilla.components.browser.state.selector.selectedTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Test

/**
 * This test suite validates that calls on [SessionManager] update [BrowserStore] to create a matching state.
 */
class SessionManagerMigrationTest {
    @Test
    fun `Add session`() {
        val state = BrowserState()
        val store = BrowserStore(state)

        val sessionManager = SessionManager(engine = mock(), store = store)

        assertTrue(sessionManager.sessions.isEmpty())
        assertTrue(store.state.tabs.isEmpty())

        sessionManager.add(Session("https://www.mozilla.org", private = true))

        assertEquals(1, sessionManager.sessions.size)
        assertEquals(1, store.state.tabs.size)

        val tab = store.state.tabs[0]

        assertEquals("https://www.mozilla.org", tab.content.url)
        assertTrue(tab.content.private)
    }

    @Test
    fun `Remove session`() {
        val state = BrowserState()
        val store = BrowserStore(state)

        val sessionManager = SessionManager(engine = mock(), store = store)

        sessionManager.add(Session("https://www.mozilla.org"))
        sessionManager.add(Session("https://www.firefox.com"))

        assertEquals(2, sessionManager.sessions.size)
        assertEquals(2, store.state.tabs.size)

        sessionManager.remove(sessionManager.sessions[0])

        assertEquals(1, sessionManager.sessions.size)
        assertEquals(1, store.state.tabs.size)

        assertEquals("https://www.firefox.com", store.state.tabs[0].content.url)
    }

    @Test
    fun `Selecting session`() {
        val state = BrowserState()
        val store = BrowserStore(state)

        val sessionManager = SessionManager(engine = mock(), store = store)

        sessionManager.add(Session("https://www.mozilla.org"))
        sessionManager.add(Session("https://www.firefox.com"))

        sessionManager.select(sessionManager.sessions[1])

        val selectedTab = store.state.selectedTab
        assertNotNull(selectedTab!!)

        assertEquals("https://www.firefox.com", sessionManager.selectedSessionOrThrow.url)
        assertEquals("https://www.firefox.com", selectedTab.content.url)
    }
}
