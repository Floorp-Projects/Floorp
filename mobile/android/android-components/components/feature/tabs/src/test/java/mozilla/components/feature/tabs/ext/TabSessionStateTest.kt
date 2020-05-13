/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs.ext

import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.MediaState
import mozilla.components.browser.state.state.createTab
import mozilla.components.concept.engine.media.Media
import org.junit.Test
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull

class TabSessionStateTest {
    @Test
    fun `mediaState gets set correctly`() {
        var browserState = BrowserState(
            tabs = listOf(
                createTab("https://www.mozilla.org", id = "a"),
                createTab("https://getpocket.com", id = "b"),
                createTab("https://developer.mozilla.org", id = "c"),
                createTab("https://www.firefox.com", id = "d"),
                createTab("https://www.google.com", id = "e")
            ),
            selectedTabId = "a",
            media = MediaState(MediaState.Aggregate(activeTabId = "a", state = MediaState.State.PLAYING))
        )

        var tabs = browserState.toTabs()
        assertEquals(Media.State.PLAYING, tabs.list[0].mediaState)

        browserState = browserState.copy(
            media = MediaState(MediaState.Aggregate(activeTabId = "b", state = MediaState.State.PAUSED))
        )
        tabs = browserState.toTabs()
        assertNull(tabs.list[0].mediaState)
        assertEquals(Media.State.PAUSED, tabs.list[1].mediaState)
    }
}
