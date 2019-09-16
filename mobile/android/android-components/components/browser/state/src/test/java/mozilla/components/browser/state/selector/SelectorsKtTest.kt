/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.selector

import mozilla.components.browser.state.action.CustomTabListAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.state.createCustomTab
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.ext.joinBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Test

class SelectorsKtTest {
    @Test
    fun `selectedTab extension property`() {
        val store = BrowserStore()

        assertNull(store.state.selectedTab)

        store.dispatch(
            CustomTabListAction.AddCustomTabAction(createCustomTab("https://www.mozilla.org"))
        ).joinBlocking()

        assertNull(store.state.selectedTab)

        val tab = createTab("https://www.firefox.com")
        store.dispatch(TabListAction.AddTabAction(tab, select = true)).joinBlocking()

        assertEquals(tab, store.state.selectedTab)

        val otherTab = createTab("https://getpocket.com")
        store.dispatch(TabListAction.AddTabAction(otherTab)).joinBlocking()

        assertEquals(tab, store.state.selectedTab)

        store.dispatch(TabListAction.SelectTabAction(otherTab.id)).joinBlocking()

        assertEquals(otherTab, store.state.selectedTab)
    }

    @Test
    fun `selectedTab extension property - ignores unknown id`() {
        val state = BrowserState(
            selectedTabId = "no valid id"
        )

        assertNull(state.selectedTab)
    }

    @Test
    fun `findTab extension function`() {
        val tab = createTab("https://www.firefox.com")
        val otherTab = createTab("https://getpocket.com")
        val customTab = createCustomTab("https://www.mozilla.org")

        val state = BrowserState(
            tabs = listOf(tab, otherTab),
            customTabs = listOf(customTab))

        assertEquals(tab, state.findTab(tab.id))
        assertEquals(otherTab, state.findTab(otherTab.id))
        assertNull(state.findTab(customTab.id))
    }

    @Test
    fun `findTabCustomTab extension function`() {
        val tab = createTab("https://www.firefox.com")
        val otherTab = createTab("https://getpocket.com")
        val customTab = createCustomTab("https://www.mozilla.org")

        val state = BrowserState(
            tabs = listOf(tab, otherTab),
            customTabs = listOf(customTab))

        assertNull(state.findCustomTab(tab.id))
        assertNull(state.findCustomTab(otherTab.id))
        assertEquals(customTab, state.findCustomTab(customTab.id))
    }

    @Test
    fun `findCustomTabOrSelectedTab extension function`() {
        val tab = createTab("https://www.firefox.com")
        val otherTab = createTab("https://getpocket.com")
        val customTab = createCustomTab("https://www.mozilla.org")

        val state = BrowserState(
            tabs = listOf(tab, otherTab),
            customTabs = listOf(customTab),
            selectedTabId = tab.id)

        assertEquals(tab, state.findCustomTabOrSelectedTab())
        assertEquals(tab, state.findCustomTabOrSelectedTab(null))
        assertEquals(customTab, state.findCustomTabOrSelectedTab(customTab.id))
        assertNull(state.findCustomTabOrSelectedTab(tab.id))
        assertNull(state.findCustomTabOrSelectedTab(otherTab.id))
    }

    @Test
    fun `privateTabs and normalTabs extension properties`() {
        val tab1 = createTab("https://www.firefox.com")
        val tab2 = createTab("https://www.mozilla.org")
        val privateTab1 = createTab("https://getpocket.com", private = true)
        val privateTab2 = createTab("https://www.example.org", private = true)

        val state = BrowserState(
            tabs = listOf(tab1, privateTab1, tab2, privateTab2),
            customTabs = listOf(createCustomTab("https://www.google.com")))

        assertEquals(listOf(tab1, tab2), state.normalTabs)
        assertEquals(listOf(privateTab1, privateTab2), state.privateTabs)

        assertEquals(emptyList<TabSessionState>(), BrowserState().normalTabs)
        assertEquals(emptyList<TabSessionState>(), BrowserState().privateTabs)
    }

    @Test
    fun `findTabOrCustomTab finds regular and custom tabs`() {
        BrowserState(
            tabs = listOf(createTab("https://www.mozilla.org", id = "test-id"))
        ).also { state ->
            assertNotNull(state.findTabOrCustomTab("test-id"))
            assertEquals(
                "https://www.mozilla.org",
                state.findTabOrCustomTab("test-id")!!.content.url)
        }

        BrowserState(
            customTabs = listOf(createCustomTab("https://www.mozilla.org", id = "test-id"))
        ).also { state ->
            assertNotNull(state.findTabOrCustomTab("test-id"))
            assertEquals(
                "https://www.mozilla.org",
                state.findTabOrCustomTab("test-id")!!.content.url)
        }
    }
}
