/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import mozilla.components.browser.state.action.LastAccessAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.state.LastMediaAccessState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class LastAccessReducerTest {
    @Test
    fun `WHEN the reducer is called for UpdateLastAccessAction THEN a new state with updated lastAccess is returned`() {
        val tab1 = TabSessionState(id = "tab1", lastAccess = 111, content = mock())
        val tab2 = TabSessionState(id = "tab2", lastAccess = 222, content = mock())
        val browserState = BrowserState(tabs = listOf(tab1, tab2))

        val updatedState = LastAccessReducer.reduce(
            browserState,
            LastAccessAction.UpdateLastAccessAction(tabId = "tab1", lastAccess = 345),
        )

        assertEquals(2, updatedState.tabs.size)
        assertEquals(345, updatedState.tabs[0].lastAccess)
        assertEquals(222, updatedState.tabs[1].lastAccess)
    }

    @Test
    fun `WHEN the reducer is called for UpdateLastMediaAccessAction THEN a new state with updated LastMediaAccessState is returned`() {
        val tab1 = TabSessionState(id = "tab1", content = ContentState("https://mozilla.org"))
        val tab2 = TabSessionState(id = "tab2", content = mock())
        val browserState = BrowserState(tabs = listOf(tab1, tab2))

        val updatedState = LastAccessReducer.reduce(
            browserState,
            LastAccessAction.UpdateLastMediaAccessAction(tabId = "tab1", lastMediaAccess = 345),
        )

        assertEquals(2, updatedState.tabs.size)
        assertEquals(345, updatedState.tabs[0].lastMediaAccessState.lastMediaAccess)
        assertTrue(updatedState.tabs[0].lastMediaAccessState.mediaSessionActive)
        assertEquals("https://mozilla.org", updatedState.tabs[0].lastMediaAccessState.lastMediaUrl)
        assertEquals(0, updatedState.tabs[1].lastMediaAccessState.lastMediaAccess)
        assertEquals("", updatedState.tabs[1].lastMediaAccessState.lastMediaUrl)
        assertFalse(updatedState.tabs[1].lastMediaAccessState.mediaSessionActive)
    }

    @Test
    fun `WHEN the reducer is called for UpdateLastMediaAccessAction on a custom tab THEN the BrowserState is not updated`() {
        val normalTab = TabSessionState(id = "tab1", content = ContentState(url = "https:mozilla.org"))
        val privateTab = TabSessionState(id = "tab2", content = ContentState(url = "https:mozilla.org", private = true))
        val customTab = CustomTabSessionState(
            id = "tab3",
            content = ContentState(url = "https://mozilla.org"),
            config = mock(),
        )
        val browserState = BrowserState(tabs = listOf(normalTab, privateTab), customTabs = listOf(customTab))

        val updatedState = LastAccessReducer.reduce(
            browserState,
            LastAccessAction.UpdateLastMediaAccessAction(tabId = "tab3", lastMediaAccess = 345),
        )

        assertEquals(2, updatedState.tabs.size)
        assertEquals(1, updatedState.customTabs.size)
        assertEquals(normalTab, updatedState.tabs[0])
        assertEquals(privateTab, updatedState.tabs[1])
        assertEquals(customTab, updatedState.customTabs[0])
    }

    @Test
    fun `WHEN the reducer is called for ResetLastMediaSessionAction THEN a new state with a false mediaSessionActive is returned`() {
        val tab1 = TabSessionState(id = "tab1", content = mock())
        val tab2 = TabSessionState(
            id = "tab2",
            content = mock(),
            lastMediaAccessState = LastMediaAccessState("https://mozilla.org", 222, true),
        )
        val browserState = BrowserState(tabs = listOf(tab1, tab2))

        val updatedState = LastAccessReducer.reduce(
            browserState,
            LastAccessAction.ResetLastMediaSessionAction(tabId = "tab2"),
        )

        assertEquals(2, updatedState.tabs.size)
        assertEquals(0, updatedState.tabs[0].lastMediaAccessState.lastMediaAccess)
        assertEquals("", updatedState.tabs[0].lastMediaAccessState.lastMediaUrl)
        assertEquals(222, updatedState.tabs[1].lastMediaAccessState.lastMediaAccess)
        assertEquals("https://mozilla.org", updatedState.tabs[1].lastMediaAccessState.lastMediaUrl)
        assertFalse(updatedState.tabs[1].lastMediaAccessState.mediaSessionActive)
    }

    @Test
    fun `WHEN the reducer is called for ResetLastMediaSessionAction on a custom tab THEN the BrowserState is not updated`() {
        val normalTab = TabSessionState(
            id = "tab2",
            content = mock(),
            lastMediaAccessState = LastMediaAccessState("https://mozilla.org", 222, true),
        )
        val privateTab = TabSessionState(id = "tab2", content = ContentState(url = "https:mozilla.org", private = true))
        val customTab = CustomTabSessionState(
            id = "tab3",
            content = ContentState(url = "https://mozilla.org"),
            config = mock(),
        )
        val browserState = BrowserState(tabs = listOf(normalTab, privateTab), customTabs = listOf(customTab))

        val updatedState = LastAccessReducer.reduce(
            browserState,
            LastAccessAction.UpdateLastMediaAccessAction(tabId = "tab3", lastMediaAccess = 345),
        )

        assertEquals(2, updatedState.tabs.size)
        assertEquals(1, updatedState.customTabs.size)
        assertEquals(normalTab, updatedState.tabs[0])
        assertEquals(privateTab, updatedState.tabs[1])
        assertEquals(customTab, updatedState.customTabs[0])
    }
}
