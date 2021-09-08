/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import io.mockk.mockk
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.LastMediaAccessState
import mozilla.components.browser.state.state.createTab
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class BrowserStateTest {

    @Test
    fun `GIVEN a tab which had media playing WHEN inProgressMediaTab is called THEN return that tab`() {
        val inProgressMediaTab = createTab(
            url = "mediaUrl", id = "2",
            lastMediaAccessState = LastMediaAccessState("https://mozilla.com", 123, true)
        )
        val browserState = BrowserState(
            tabs = listOf(mockk(relaxed = true), inProgressMediaTab, mockk(relaxed = true))
        )

        assertEquals(inProgressMediaTab, browserState.inProgressMediaTab)
    }

    @Test
    fun `GIVEN no tab which had media playing exists WHEN inProgressMediaTab is called THEN return null`() {
        val browserState = BrowserState(
            tabs = listOf(createTab("tab1"), createTab("tab2"), createTab("tab3"))
        )

        assertNull(browserState.inProgressMediaTab)
    }

    @Test
    fun `GIVEN the selected tab is a normal tab and no media tab exists WHEN asRecentTabs is called THEN return a list of that tab`() {
        val selectedTab = createTab(url = "url", id = "3")
        val browserState = BrowserState(
            tabs = listOf(createTab("tab1"), selectedTab, createTab("tab3")),
            selectedTabId = selectedTab.id
        )

        val result = browserState.asRecentTabs()

        assertEquals(1, result.size)
        assertEquals(selectedTab, result[0])
    }

    @Test
    fun `GIVEN the selected tab is a private tab and no media tab exists WHEN asRecentTabs is called THEN return a list of the last accessed normal tab`() {
        val selectedPrivateTab = createTab(url = "url", id = "1", lastAccess = 1, private = true)
        val lastAccessedNormalTab = createTab(url = "url2", id = "2", lastAccess = 2)
        val browserState = BrowserState(
            tabs = listOf(
                createTab("https://mozilla.org"),
                lastAccessedNormalTab,
                selectedPrivateTab
            ),
            selectedTabId = selectedPrivateTab.id
        )

        val result = browserState.asRecentTabs()

        assertEquals(1, result.size)
        assertEquals(lastAccessedNormalTab, result[0])
    }

    @Test
    fun `GIVEN the selected tab is a normal tab and another media tab exists WHEN asRecentTabs is called THEN return a list of these tabs`() {
        val selectedTab = createTab(url = "url", id = "3")
        val mediaTab = createTab(
            "mediaUrl", id = "23",
            lastMediaAccessState = LastMediaAccessState("https://mozilla.com", 123, true)
        )
        val browserState = BrowserState(
            tabs = listOf(mockk(relaxed = true), selectedTab, mediaTab),
            selectedTabId = selectedTab.id
        )

        val result = browserState.asRecentTabs()

        assertEquals(2, result.size)
        assertEquals(selectedTab, result[0])
        assertEquals(mediaTab, result[1])
    }

    @Test
    fun `GIVEN the selected tab is a private tab and another media tab exists WHEN asRecentTabs is called THEN return a list of the last normal tab and the media tab`() {
        val lastAccessedNormalTab = createTab(url = "url2", id = "2", lastAccess = 2)
        val selectedPrivateTab = createTab(url = "url", id = "1", lastAccess = 1, private = true)
        val mediaTab = createTab(
            "mediaUrl", id = "12", lastAccess = 0,
            lastMediaAccessState = LastMediaAccessState("https://mozilla.com", 123, true)
        )
        val browserState = BrowserState(
            tabs = listOf(
                mockk(relaxed = true),
                lastAccessedNormalTab,
                selectedPrivateTab,
                mediaTab
            ),
            selectedTabId = selectedPrivateTab.id
        )

        val result = browserState.asRecentTabs()

        assertEquals(2, result.size)
        assertEquals(lastAccessedNormalTab, result[0])
        assertEquals(mediaTab, result[1])
    }

    @Test
    fun `GIVEN the selected tab is a private tab and the media tab is the last accessed normal tab WHEN asRecentTabs is called THEN return a list of the media tab and the second-to-last normal tab`() {
        val selectedPrivateTab = createTab(url = "url", id = "1", lastAccess = 1, private = true)
        val normalTab = createTab(url = "url2", id = "2", lastAccess = 2)
        val mediaTab = createTab(
            "mediaUrl", id = "12", lastAccess = 20,
            lastMediaAccessState = LastMediaAccessState("https://mozilla.com", 123, true)
        )
        val browserState = BrowserState(
            tabs = listOf(mockk(relaxed = true), normalTab, selectedPrivateTab, mediaTab),
            selectedTabId = selectedPrivateTab.id
        )

        val result = browserState.asRecentTabs()

        assertEquals(2, result.size)
        assertEquals(mediaTab, result[0])
    }

    @Test
    fun `GIVEN only private tabs and a private one selected WHEN lastOpenedNormalTab is called THEN return null`() {
        val selectedPrivateTab = createTab(url = "url", id = "1", private = true)
        val otherPrivateTab = createTab(url = "url2", id = "2", private = true)
        val browserState = BrowserState(
            tabs = listOf(selectedPrivateTab, otherPrivateTab),
            selectedTabId = "1"
        )

        assertNull(browserState.lastOpenedNormalTab)
    }

    @Test
    fun `GIVEN normal tabs exists but a private one is selected WHEN lastOpenedNormalTab is called THEN return the last accessed normal tab`() {
        val selectedPrivateTab = createTab(url = "url", id = "1", private = true)
        val normalTab1 = createTab(url = "url2", id = "2", private = false, lastAccess = 2)
        val normalTab2 = createTab(url = "url3", id = "3", private = false, lastAccess = 3)
        val browserState = BrowserState(
            tabs = listOf(selectedPrivateTab, normalTab1, normalTab2),
            selectedTabId = "3"
        )

        assertEquals(normalTab2, browserState.lastOpenedNormalTab)
    }

    @Test
    fun `GIVEN a normal tab is selected WHEN lastOpenedNormalTab is called THEN return the selected normal tab`() {
        val normalTab1 = createTab(url = "url1", id = "1", private = false)
        val normalTab2 = createTab(url = "url2", id = "2", private = false)
        val browserState = BrowserState(
            tabs = listOf(normalTab1, normalTab2),
            selectedTabId = "1"
        )

        assertEquals(normalTab1, browserState.lastOpenedNormalTab)
    }

    @Test
    fun `GIVEN no normal tabs are open WHEN secondToLastOpenedNormalTab is called THEN return null`() {
        val browserState = BrowserState(
            tabs = listOf(mockk(relaxed = true)),
        )
        assertNull(browserState.secondToLastOpenedNormalTab)
    }

    @Test
    fun `GIVEN one normal tab is open WHEN secondToLastOpenedNormalTab is called THEN return the one tab`() {
        val lastAccessedNormalTab = createTab(url = "url2", id = "2", lastAccess = 1)
        val browserState = BrowserState(
            tabs = listOf(lastAccessedNormalTab),
        )
        assertNull(browserState.secondToLastOpenedNormalTab)
    }

    @Test
    fun `GIVEN two normal tabs are open WHEN secondToLastOpenedNormalTab is called THEN return the second-to-last opened tab`() {
        val normalTab1 = createTab(url = "url1", id = "1", lastAccess = 1)
        val normalTab2 = createTab(url = "url2", id = "2", lastAccess = 2)
        val browserState = BrowserState(
            tabs = listOf(normalTab1, normalTab2),
        )
        assertEquals(normalTab1.id, browserState.secondToLastOpenedNormalTab!!.id)
    }

    @Test
    fun `GIVEN four normal tabs are open WHEN secondToLastOpenedNormalTab is called THEN return the second-to-last opened tab`() {
        val normalTab1 = createTab(url = "url1", id = "1", lastAccess = 1)
        val normalTab2 = createTab(url = "url2", id = "2", lastAccess = 4)
        val normalTab3 = createTab(url = "url3", id = "3", lastAccess = 3)
        val normalTab4 = createTab(url = "url4", id = "4", lastAccess = 2)
        val browserState = BrowserState(
            tabs = listOf(normalTab1, normalTab2, normalTab3, normalTab4),
        )
        assertEquals(normalTab3.id, browserState.secondToLastOpenedNormalTab!!.id)
    }
}
