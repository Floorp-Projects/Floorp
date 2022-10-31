/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.tabs.ext

import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.LastMediaAccessState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createTab
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class TabSessionStateTest {
    @Test
    fun `GIVEN lastMediaUrl is the same as the current tab url and mediaSessionActive is true WHEN hasMediaPlayed is called THEN return true`() {
        val tab = TabSessionState(
            content = ContentState(url = "https://mozilla.org"),
            lastMediaAccessState = LastMediaAccessState(lastMediaUrl = "https://mozilla.org", mediaSessionActive = true),
        )

        assertTrue(tab.hasMediaPlayed())
    }

    @Test
    fun `GIVEN lastMediaUrl is the same as the current tab url and and mediaSessionActive is false WHEN hasMediaPlayed is called THEN return true`() {
        val tab = TabSessionState(
            content = ContentState(url = "https://mozilla.org"),
            lastMediaAccessState = LastMediaAccessState(lastMediaUrl = "https://mozilla.org", mediaSessionActive = false),
        )

        assertTrue(tab.hasMediaPlayed())
    }

    @Test
    fun `GIVEN lastMediaUrl is different than the current tab url and mediaSessionActive is false WHEN hasMediaPlayed is called THEN return false`() {
        val tab = TabSessionState(
            content = ContentState(url = "https://mozilla.org"),
            lastMediaAccessState = LastMediaAccessState(lastMediaUrl = "https://firefox.com", mediaSessionActive = false),
        )

        assertFalse(tab.hasMediaPlayed())
    }

    @Test
    fun `WHEN creating a new TabSessionState THEN createAt is initialized with currentTimeMillis`() {
        val currentTime = System.currentTimeMillis()

        val newTab = TabSessionState(content = ContentState(url = "https://mozilla.org"))
        val newTab2 = createTab(url = "https://mozilla.org")

        assertTrue(currentTime <= newTab.createdAt)
        assertTrue(currentTime <= newTab2.createdAt)
    }
}
