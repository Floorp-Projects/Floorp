/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import io.mockk.mockk
import mozilla.components.browser.session.Session
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ReaderState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.publicsuffixlist.PublicSuffixList
import org.junit.Assert.assertEquals
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class SessionTest {

    @Test
    fun `toTab uses active reader URL`() {
        val sessionWithoutReaderState = Session(id = "1", initialUrl = "https://example.com")
        val tabWithoutReaderState = createTab(url = sessionWithoutReaderState.url, id = sessionWithoutReaderState.id)

        val sessionWithInactiveReaderState = Session(id = "2", initialUrl = "https://blog.mozilla.org")
        val tabWithInactiveReaderState = createTab(url = sessionWithInactiveReaderState.url, id = sessionWithInactiveReaderState.id,
            readerState = ReaderState(active = false, activeUrl = null)
        )

        val sessionWithActiveReaderState = Session(id = "3", initialUrl = "moz-extension://123")
        val tabWithActiveReaderState = createTab(url = sessionWithActiveReaderState.url, id = sessionWithActiveReaderState.id,
            readerState = ReaderState(active = true, activeUrl = "https://blog.mozilla.org/123")
        )

        val tabs = listOf(tabWithoutReaderState, tabWithInactiveReaderState, tabWithActiveReaderState)
        val store = BrowserStore(BrowserState(tabs))

        val suffixList = mockk<PublicSuffixList>(relaxed = true)
        assertEquals(sessionWithoutReaderState.url, sessionWithoutReaderState.toTab(store, suffixList).url)
        assertEquals(sessionWithInactiveReaderState.url, sessionWithInactiveReaderState.toTab(store, suffixList).url)
        assertEquals("https://blog.mozilla.org/123", sessionWithActiveReaderState.toTab(store, suffixList).url)
    }
}
