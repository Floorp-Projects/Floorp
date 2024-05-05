/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.ext

import mozilla.components.browser.state.state.ReaderState
import mozilla.components.browser.state.state.createTab
import org.junit.Assert.assertEquals
import org.junit.Test

class TabSessionStateTest {

    @Test
    fun `GIVEN reader mode is active WHEN get url extension property is fetched THEN return the active url`() {
        val readerUrl = "moz-extension://1234"
        val activeUrl = "https://mozilla.org"
        val readerTab = createTab(
            url = readerUrl,
            readerState = ReaderState(active = true, activeUrl = activeUrl),
            title = "Mozilla",
        )

        assertEquals(activeUrl, readerTab.getUrl())
    }

    @Test
    fun `WHEN get url extension property is fetched THEN return the content url`() {
        val url = "https://mozilla.org"
        val tab = createTab(url = url)

        assertEquals(url, tab.getUrl())
    }
}
