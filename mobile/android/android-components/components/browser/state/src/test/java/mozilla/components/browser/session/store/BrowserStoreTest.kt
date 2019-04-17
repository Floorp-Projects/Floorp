/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.store

import kotlinx.coroutines.runBlocking
import mozilla.components.browser.session.action.SessionListAction
import mozilla.components.browser.session.state.BrowserState
import mozilla.components.browser.session.state.SessionState
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class BrowserStoreTest {
    @Test
    fun `Adding SessionState`() = runBlocking {
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
}
