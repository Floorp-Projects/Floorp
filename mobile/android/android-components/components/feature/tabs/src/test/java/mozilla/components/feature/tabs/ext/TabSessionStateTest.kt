/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs.ext

import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.LastMediaAccessState
import mozilla.components.browser.state.state.TabSessionState
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class TabSessionStateTest {
    @Test
    fun `GIVEN lastMediaUrl is the same as the current tab url and lastMediaAccess is 0 WHEN hasMediaPlayed is called THEN return true`() {
        val tab = TabSessionState(
            content = ContentState(url = "https://mozilla.org"),
            lastMediaAccessState = LastMediaAccessState("https://mozilla.org", 0)
        )

        assertTrue(tab.hasMediaPlayed())
    }

    @Test
    fun `GIVEN lastMediaUrl is the same as the current tab url and lastMediaAccess has a positive value WHEN hasMediaPlayed is called THEN return true`() {
        val tab = TabSessionState(
            content = ContentState(url = "https://mozilla.org"),
            lastMediaAccessState = LastMediaAccessState("https://mozilla.org", 3)
        )

        assertTrue(tab.hasMediaPlayed())
    }

    @Test
    fun `GIVEN lastMediaUrl is different than the current tab url and lastMediaAccess is 0 WHEN hasMediaPlayed is called THEN return false`() {
        val tab = TabSessionState(
            content = ContentState(url = "https://mozilla.org"),
            lastMediaAccessState = LastMediaAccessState("https://firefox.com", 0)
        )

        assertFalse(tab.hasMediaPlayed())
    }
}
