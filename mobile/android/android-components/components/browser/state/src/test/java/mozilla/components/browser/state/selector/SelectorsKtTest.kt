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
        val tabLastAccessTimeStamp = 123L

        assertNull(store.state.selectedTab)

        store.dispatch(
            CustomTabListAction.AddCustomTabAction(createCustomTab("https://www.mozilla.org")),
        ).joinBlocking()

        assertNull(store.state.selectedTab)

        val tab = createTab("https://www.firefox.com")
        store.dispatch(TabListAction.AddTabAction(tab, select = true)).joinBlocking()

        assertEquals(tab, store.state.selectedTab)

        val otherTab = createTab("https://getpocket.com", lastAccess = tabLastAccessTimeStamp)
        store.dispatch(TabListAction.AddTabAction(otherTab)).joinBlocking()

        assertEquals(tab, store.state.selectedTab)

        store.dispatch(TabListAction.SelectTabAction(otherTab.id)).joinBlocking()

        assertEquals(otherTab, store.state.selectedTab)
    }

    @Test
    fun `selectedNormalTab extension property`() {
        val store = BrowserStore()
        val tabLastAccessTimeStamp = 123L
        assertNull(store.state.selectedNormalTab)

        store.dispatch(
            CustomTabListAction.AddCustomTabAction(createCustomTab("https://www.mozilla.org")),
        ).joinBlocking()

        assertNull(store.state.selectedNormalTab)

        val privateTab = createTab("https://www.firefox.com", private = true)
        store.dispatch(TabListAction.AddTabAction(privateTab, select = true)).joinBlocking()
        assertNull(store.state.selectedNormalTab)

        val normalTab = createTab("https://getpocket.com", lastAccess = tabLastAccessTimeStamp)
        store.dispatch(TabListAction.AddTabAction(normalTab)).joinBlocking()
        assertNull(store.state.selectedNormalTab)

        store.dispatch(TabListAction.SelectTabAction(normalTab.id)).joinBlocking()
        assertEquals(normalTab, store.state.selectedNormalTab)
    }

    @Test
    fun `selectedTab extension property - ignores unknown id`() {
        val state = BrowserState(
            selectedTabId = "no valid id",
        )

        assertNull(state.selectedTab)
    }

    @Test
    fun `selectedNormalTab extension property - ignores unknown id`() {
        val state = BrowserState(
            selectedTabId = "no valid id",
        )

        assertNull(state.selectedNormalTab)
    }

    @Test
    fun `findTab extension function`() {
        val tab = createTab("https://www.firefox.com")
        val otherTab = createTab("https://getpocket.com")
        val customTab = createCustomTab("https://www.mozilla.org")

        val state = BrowserState(
            tabs = listOf(tab, otherTab),
            customTabs = listOf(customTab),
        )

        assertEquals(tab, state.findTab(tab.id))
        assertEquals(otherTab, state.findTab(otherTab.id))
        assertNull(state.findTab(customTab.id))
    }

    @Test
    fun `findNormalTab extension function`() {
        val privateTab = createTab("https://www.firefox.com", private = true)
        val normalTab = createTab("https://getpocket.com")
        val customTab = createCustomTab("https://www.mozilla.org")

        val state = BrowserState(
            tabs = listOf(privateTab, normalTab),
            customTabs = listOf(customTab),
        )

        assertEquals(normalTab, state.findNormalTab(normalTab.id))
        assertNull(state.findNormalTab(privateTab.id))
        assertNull(state.findNormalTab(customTab.id))
    }

    @Test
    fun `findTabCustomTab extension function`() {
        val tab = createTab("https://www.firefox.com")
        val otherTab = createTab("https://getpocket.com")
        val customTab = createCustomTab("https://www.mozilla.org")

        val state = BrowserState(
            tabs = listOf(tab, otherTab),
            customTabs = listOf(customTab),
        )

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
            selectedTabId = tab.id,
        )

        assertEquals(tab, state.findCustomTabOrSelectedTab())
        assertEquals(tab, state.findCustomTabOrSelectedTab(null))
        assertEquals(customTab, state.findCustomTabOrSelectedTab(customTab.id))
        assertNull(state.findCustomTabOrSelectedTab(tab.id))
        assertNull(state.findCustomTabOrSelectedTab(otherTab.id))
    }

    @Test
    fun `findTabOrCustomTabOrSelectedTab extension function`() {
        val tab = createTab("https://www.firefox.com")
        val otherTab = createTab("https://getpocket.com")
        val customTab = createCustomTab("https://www.mozilla.org")

        val state = BrowserState(
            tabs = listOf(tab, otherTab),
            customTabs = listOf(customTab),
            selectedTabId = tab.id,
        )

        assertEquals(tab, state.findTabOrCustomTabOrSelectedTab())
        assertEquals(tab, state.findTabOrCustomTabOrSelectedTab(null))
        assertEquals(tab, state.findTabOrCustomTabOrSelectedTab(tab.id))
        assertEquals(otherTab, state.findTabOrCustomTabOrSelectedTab(otherTab.id))
        assertEquals(customTab, state.findTabOrCustomTabOrSelectedTab(customTab.id))
    }

    @Test
    fun `getNormalOrPrivateTabs extension function`() {
        val tab1 = createTab("https://www.firefox.com")
        val tab2 = createTab("https://www.mozilla.org")
        val privateTab1 = createTab("https://getpocket.com", private = true)
        val privateTab2 = createTab("https://www.example.org", private = true)

        val state = BrowserState(
            tabs = listOf(tab1, privateTab1, tab2, privateTab2),
            customTabs = listOf(createCustomTab("https://www.google.com")),
        )

        assertEquals(listOf(tab1, tab2), state.getNormalOrPrivateTabs(private = false))
        assertEquals(listOf(privateTab1, privateTab2), state.getNormalOrPrivateTabs(private = true))

        assertEquals(emptyList<TabSessionState>(), BrowserState().getNormalOrPrivateTabs(private = true))
        assertEquals(emptyList<TabSessionState>(), BrowserState().getNormalOrPrivateTabs(private = false))
    }

    @Test
    fun `privateTabs and normalTabs extension properties`() {
        val tab1 = createTab("https://www.firefox.com")
        val tab2 = createTab("https://www.mozilla.org")
        val privateTab1 = createTab("https://getpocket.com", private = true)
        val privateTab2 = createTab("https://www.example.org", private = true)

        val state = BrowserState(
            tabs = listOf(tab1, privateTab1, tab2, privateTab2),
            customTabs = listOf(createCustomTab("https://www.google.com")),
        )

        assertEquals(listOf(tab1, tab2), state.normalTabs)
        assertEquals(listOf(privateTab1, privateTab2), state.privateTabs)

        assertEquals(emptyList<TabSessionState>(), BrowserState().normalTabs)
        assertEquals(emptyList<TabSessionState>(), BrowserState().privateTabs)
    }

    @Test
    fun `findTabOrCustomTab finds normal and custom tabs`() {
        BrowserState(
            tabs = listOf(createTab("https://www.mozilla.org", id = "test-id")),
        ).also { state ->
            assertNotNull(state.findTabOrCustomTab("test-id"))
            assertEquals(
                "https://www.mozilla.org",
                state.findTabOrCustomTab("test-id")!!.content.url,
            )
        }

        BrowserState(
            customTabs = listOf(createCustomTab("https://www.mozilla.org", id = "test-id")),
        ).also { state ->
            assertNotNull(state.findTabOrCustomTab("test-id"))
            assertEquals(
                "https://www.mozilla.org",
                state.findTabOrCustomTab("test-id")!!.content.url,
            )
        }
    }

    @Test
    fun `findNormalOrPrivateTabByUrl finds a matching normal tab`() {
        BrowserState(
            tabs = listOf(createTab("https://www.mozilla.org", id = "test-id", private = false)),
        ).also { state ->
            assertNotNull(state.findNormalOrPrivateTabByUrl("https://www.mozilla.org", false))
            assertEquals(
                "https://www.mozilla.org",
                state.findNormalOrPrivateTabByUrl("https://www.mozilla.org", false)!!.content.url,
            )
            assertEquals(
                "test-id",
                state.findNormalOrPrivateTabByUrl("https://www.mozilla.org", false)!!.id,
            )
        }
    }

    @Test
    fun `findNormalOrPrivateTabByUrl finds no matching normal tab`() {
        BrowserState(
            tabs = listOf(createTab("https://www.mozilla.org", id = "test-id", private = true)),
        ).also { state ->
            assertNull(state.findNormalOrPrivateTabByUrl("https://www.mozilla.org", false))
        }
    }

    @Test
    fun `findNormalOrPrivateTabByUrl finds a matching private tab`() {
        BrowserState(
            tabs = listOf(createTab("https://www.mozilla.org", id = "test-id", private = true)),
        ).also { state ->
            assertNotNull(state.findNormalOrPrivateTabByUrl("https://www.mozilla.org", true))
            assertEquals(
                "https://www.mozilla.org",
                state.findNormalOrPrivateTabByUrl("https://www.mozilla.org", true)!!.content.url,
            )
            assertEquals(
                "test-id",
                state.findNormalOrPrivateTabByUrl("https://www.mozilla.org", true)!!.id,
            )
        }
    }

    @Test
    fun `findNormalOrPrivateTabByUrl finds no matching private tab`() {
        BrowserState(
            tabs = listOf(createTab("https://www.mozilla.org", id = "test-id", private = false)),
        ).also { state ->
            assertNull(state.findNormalOrPrivateTabByUrl("https://www.mozilla.org", true))
        }
    }
}
