/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.CustomTabSessionState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Test

class BrowserStateReducerKtTest {
    private val initialUrl = "https://mozilla.com"
    private val updatedUrl = "https://firefox.com"

    @Test
    fun `GIVEN tabs and custom tabs exist WHEN updateTabOrCustomTabState is called with unknown id THEN no tab is updated`() {
        val normalTab = TabSessionState(id = "tab1", content = ContentState(url = initialUrl))
        val privateTab = TabSessionState(id = "tab2", content = ContentState(url = initialUrl, private = true))
        val customTab = CustomTabSessionState(
            id = "tab3",
            content = ContentState(url = initialUrl),
            config = mock(),
        )
        var browserState = BrowserState(tabs = listOf(normalTab, privateTab), customTabs = listOf(customTab))

        browserState = browserState.updateTabOrCustomTabState("tab1111") {
            it.createCopy(content = it.content.copy(url = updatedUrl))
        }

        assertEquals(2, browserState.tabs.size)
        assertEquals(1, browserState.customTabs.size)
        assertEquals(initialUrl, browserState.tabs[0].content.url)
        assertEquals(initialUrl, browserState.tabs[1].content.url)
        assertEquals(initialUrl, browserState.customTabs[0].content.url)
    }

    @Test
    fun `GIVEN a normal tab exists WHEN updateTabOrCustomTabState is called with the id of that tab THEN the tab is updated`() {
        val tab1 = TabSessionState(id = "tab1", content = ContentState(url = initialUrl))
        val tab2 = TabSessionState(id = "tab2", content = ContentState(url = initialUrl))
        var browserState = BrowserState(tabs = listOf(tab1, tab2))

        browserState = browserState.updateTabOrCustomTabState("tab1") {
            it.createCopy(content = it.content.copy(url = updatedUrl))
        }

        assertEquals(2, browserState.tabs.size)
        assertEquals(updatedUrl, browserState.tabs[0].content.url)
        assertEquals(initialUrl, browserState.tabs[1].content.url)
    }

    @Test
    fun `GIVEN a private tab exists WHEN updateTabOrCustomTabState is called with the id of that tab THEN the tab is updated`() {
        val tab1 = TabSessionState(id = "tab1", content = ContentState(url = initialUrl, private = true))
        val tab2 = TabSessionState(id = "tab2", content = ContentState(url = initialUrl, private = true))
        var browserState = BrowserState(tabs = listOf(tab1, tab2))

        browserState = browserState.updateTabOrCustomTabState("tab1") {
            it.createCopy(content = it.content.copy(url = updatedUrl))
        }

        assertEquals(2, browserState.tabs.size)
        assertEquals(updatedUrl, browserState.tabs[0].content.url)
        assertEquals(initialUrl, browserState.tabs[1].content.url)
    }

    @Test
    fun `GIVEN a normal custom tab exists WHEN updateTabOrCustomTabState is called with the id of that tab THEN the tab is updated`() {
        val tab1 = CustomTabSessionState(id = "tab1", content = ContentState(url = initialUrl), config = mock())
        val tab2 = CustomTabSessionState(id = "tab2", content = ContentState(url = initialUrl), config = mock())
        var browserState = BrowserState(customTabs = listOf(tab1, tab2))

        browserState = browserState.updateTabOrCustomTabState("tab1") {
            it.createCopy(content = it.content.copy(url = updatedUrl))
        }

        assertEquals(2, browserState.customTabs.size)
        assertEquals(updatedUrl, browserState.customTabs[0].content.url)
        assertEquals(initialUrl, browserState.customTabs[1].content.url)
    }

    @Test
    fun `GIVEN a private custom tab exists WHEN updateTabOrCustomTabState is called with the id of that tab THEN the tab is updated`() {
        val tab1 = CustomTabSessionState(
            id = "tab1",
            content = ContentState(url = initialUrl, private = true),
            config = mock(),
        )
        val tab2 = CustomTabSessionState(
            id = "tab2",
            content = ContentState(url = initialUrl, private = true),
            config = mock(),
        )
        var browserState = BrowserState(customTabs = listOf(tab1, tab2))

        browserState = browserState.updateTabOrCustomTabState("tab2") {
            it.createCopy(content = it.content.copy(url = updatedUrl))
        }

        assertEquals(2, browserState.customTabs.size)
        assertEquals(initialUrl, browserState.customTabs[0].content.url)
        assertEquals(updatedUrl, browserState.customTabs[1].content.url)
    }

    @Test
    fun `GIVEN tabs and custom tabs exist WHEN updateTabState is called with the id of the custom tab THEN the custom tab is not updated`() {
        val normalTab = TabSessionState(id = "tab1", content = ContentState(url = initialUrl, private = true))
        val privateTab = TabSessionState(id = "tab2", content = ContentState(url = initialUrl))
        val customTab = CustomTabSessionState(
            id = "tab3",
            content = ContentState(url = initialUrl),
            config = mock(),
        )
        var browserState = BrowserState(tabs = listOf(normalTab, privateTab), customTabs = listOf(customTab))

        browserState = browserState.updateTabState("tab3") {
            it.copy(content = it.content.copy(url = updatedUrl))
        }

        assertEquals(2, browserState.tabs.size)
        assertEquals(1, browserState.customTabs.size)
        assertEquals(initialUrl, browserState.tabs[0].content.url)
        assertEquals(initialUrl, browserState.tabs[1].content.url)
        assertEquals(initialUrl, browserState.customTabs[0].content.url)
    }

    @Test
    fun `GIVEN a normal tab exists WHEN updateTabState is called with the id of that tab THEN the tab is updated`() {
        val tab1 = TabSessionState(id = "tab1", content = ContentState(url = initialUrl))
        val tab2 = TabSessionState(id = "tab2", content = ContentState(url = initialUrl))
        var browserState = BrowserState(tabs = listOf(tab1, tab2))

        browserState = browserState.updateTabState("tab1") {
            it.copy(content = it.content.copy(url = updatedUrl))
        }

        assertEquals(2, browserState.tabs.size)
        assertEquals(updatedUrl, browserState.tabs[0].content.url)
        assertEquals(initialUrl, browserState.tabs[1].content.url)
    }

    @Test
    fun `GIVEN a private tab exists WHEN updateTabState is called with the id of that tab THEN the tab is updated`() {
        val tab1 = TabSessionState(id = "tab1", content = ContentState(url = initialUrl, private = true))
        val tab2 = TabSessionState(id = "tab2", content = ContentState(url = initialUrl, private = true))
        var browserState = BrowserState(tabs = listOf(tab1, tab2))

        browserState = browserState.updateTabState("tab1") {
            it.copy(content = it.content.copy(url = updatedUrl))
        }

        assertEquals(2, browserState.tabs.size)
        assertEquals(updatedUrl, browserState.tabs[0].content.url)
        assertEquals(initialUrl, browserState.tabs[1].content.url)
    }

    @Test
    fun `GIVEN tabs and custom tabs exist WHEN updateCustomTabState is called with the id of a normal tab THEN no tab is updated`() {
        val normalTab = TabSessionState(id = "tab1", content = ContentState(url = initialUrl, private = true))
        val privateTab = TabSessionState(id = "tab2", content = ContentState(url = initialUrl))
        val customTab = CustomTabSessionState(
            id = "tab3",
            content = ContentState(url = initialUrl),
            config = mock(),
        )
        var browserState = BrowserState(tabs = listOf(normalTab, privateTab), customTabs = listOf(customTab))

        browserState = browserState.updateCustomTabState("tab1") {
            it.copy(content = it.content.copy(url = updatedUrl))
        }

        assertEquals(2, browserState.tabs.size)
        assertEquals(1, browserState.customTabs.size)
        assertEquals(initialUrl, browserState.tabs[0].content.url)
        assertEquals(initialUrl, browserState.tabs[1].content.url)
        assertEquals(initialUrl, browserState.customTabs[0].content.url)
    }

    @Test
    fun `GIVEN tabs and custom tabs exist WHEN updateCustomTabState is called with the id of a private tab THEN no tab is updated`() {
        val normalTab = TabSessionState(id = "tab1", content = ContentState(url = initialUrl, private = true))
        val privateTab = TabSessionState(id = "tab2", content = ContentState(url = initialUrl))
        val customTab = CustomTabSessionState(
            id = "tab3",
            content = ContentState(url = initialUrl),
            config = mock(),
        )
        var browserState = BrowserState(tabs = listOf(normalTab, privateTab), customTabs = listOf(customTab))

        browserState = browserState.updateCustomTabState("tab2") {
            it.copy(content = it.content.copy(url = updatedUrl))
        }

        assertEquals(2, browserState.tabs.size)
        assertEquals(1, browserState.customTabs.size)
        assertEquals(initialUrl, browserState.tabs[0].content.url)
        assertEquals(initialUrl, browserState.tabs[1].content.url)
        assertEquals(initialUrl, browserState.customTabs[0].content.url)
    }

    @Test
    fun `GIVEN a custom tab exists WHEN updateCustomTabState is called with the id of that tab THEN the tab is updated`() {
        val tab1 = CustomTabSessionState(id = "tab1", content = ContentState(url = initialUrl), config = mock())
        val tab2 = CustomTabSessionState(id = "tab2", content = ContentState(url = initialUrl), config = mock())
        var browserState = BrowserState(customTabs = listOf(tab1, tab2))

        browserState = browserState.updateCustomTabState("tab1") {
            it.copy(content = it.content.copy(url = updatedUrl))
        }

        assertEquals(2, browserState.customTabs.size)
        assertEquals(updatedUrl, browserState.customTabs[0].content.url)
        assertEquals(initialUrl, browserState.customTabs[1].content.url)
    }

    @Test
    fun `GIVEN a private tab exists WHEN updateCustomTabState is called with the id of that tab THEN the tab is updated`() {
        val tab1 = CustomTabSessionState(
            id = "tab1",
            content = ContentState(url = initialUrl, private = true),
            config = mock(),
        )
        val tab2 = CustomTabSessionState(
            id = "tab2",
            content = ContentState(url = initialUrl, private = true),
            config = mock(),
        )
        var browserState = BrowserState(customTabs = listOf(tab1, tab2))

        browserState = browserState.updateCustomTabState("tab1") {
            it.copy(content = it.content.copy(url = updatedUrl))
        }

        assertEquals(2, browserState.customTabs.size)
        assertEquals(updatedUrl, browserState.customTabs[0].content.url)
        assertEquals(initialUrl, browserState.customTabs[1].content.url)
    }
}
