/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.action

import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createCustomTab
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.ext.joinBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class TabListActionTest {
    @Test
    fun `AddTabAction - Adds provided SessionState`() {
        val state = BrowserState()
        val store = BrowserStore(state)

        assertEquals(0, store.state.tabs.size)
        assertNull(store.state.selectedTabId)

        val tab = createTab(url = "https://www.mozilla.org")

        store.dispatch(TabListAction.AddTabAction(tab))
            .joinBlocking()

        assertEquals(1, store.state.tabs.size)
        assertEquals(tab.id, store.state.selectedTabId)
    }

    @Test
    fun `AddTabAction - Add tab and update selection`() {
        val existingTab = createTab("https://www.mozilla.org")

        val state = BrowserState(
            tabs = listOf(existingTab),
            selectedTabId = existingTab.id
        )

        val store = BrowserStore(state)

        assertEquals(1, store.state.tabs.size)
        assertEquals(existingTab.id, store.state.selectedTabId)

        val newTab = createTab("https://firefox.com")

        store.dispatch(TabListAction.AddTabAction(newTab, select = true)).joinBlocking()

        assertEquals(2, store.state.tabs.size)
        assertEquals(newTab.id, store.state.selectedTabId)
    }

    @Test
    fun `AddTabAction - Select first tab automatically`() {
        val existingTab = createTab("https://www.mozilla.org")

        val state = BrowserState()
        val store = BrowserStore(state)

        assertEquals(0, store.state.tabs.size)
        assertNull(existingTab.id, store.state.selectedTabId)

        val newTab = createTab("https://firefox.com")
        store.dispatch(TabListAction.AddTabAction(newTab, select = false)).joinBlocking()

        assertEquals(1, store.state.tabs.size)
        assertEquals(newTab.id, store.state.selectedTabId)
    }

    @Test
    fun `SelectTabAction - Selects SessionState by id`() {
        val state = BrowserState(
            tabs = listOf(
                createTab(id = "a", url = "https://www.mozilla.org"),
                createTab(id = "b", url = "https://www.firefox.com")
            )
        )
        val store = BrowserStore(state)

        assertNull(store.state.selectedTabId)

        store.dispatch(TabListAction.SelectTabAction("a"))
            .joinBlocking()

        assertEquals("a", store.state.selectedTabId)
    }

    @Test
    fun `RemoveTabAction - Removes SessionState`() {
        val state = BrowserState(
            tabs = listOf(
                createTab(id = "a", url = "https://www.mozilla.org"),
                createTab(id = "b", url = "https://www.firefox.com")
            )
        )
        val store = BrowserStore(state)

        store.dispatch(TabListAction.RemoveTabAction("a"))
            .joinBlocking()

        assertEquals(1, store.state.tabs.size)
        assertEquals("https://www.firefox.com", store.state.tabs[0].content.url)
    }

    @Test
    fun `RemoveTabAction - Noop for unknown id`() {
        val state = BrowserState(
            tabs = listOf(
                createTab(id = "a", url = "https://www.mozilla.org"),
                createTab(id = "b", url = "https://www.firefox.com")
            )
        )
        val store = BrowserStore(state)

        store.dispatch(TabListAction.RemoveTabAction("c"))
            .joinBlocking()

        assertEquals(2, store.state.tabs.size)
        assertEquals("https://www.mozilla.org", store.state.tabs[0].content.url)
        assertEquals("https://www.firefox.com", store.state.tabs[1].content.url)
    }

    @Test
    fun `RemoveTabAction - Selected tab id is set to null if selected and last tab is removed`() {
        val state = BrowserState(
            tabs = listOf(
                createTab(id = "a", url = "https://www.mozilla.org")
            ),
            selectedTabId = "a"
        )

        val store = BrowserStore(state)

        assertEquals("a", store.state.selectedTabId)

        store.dispatch(TabListAction.RemoveTabAction("a")).joinBlocking()

        assertNull(store.state.selectedTabId)
    }

    @Test
    fun `RemoveTabAction - Does not select custom tab`() {
        val state = BrowserState(
            tabs = listOf(
                createTab(id = "a", url = "https://www.mozilla.org")
            ),
            customTabs = listOf(
                createCustomTab(id = "b", url = "https://www.firefox.com")
            ),
            selectedTabId = "a"
        )

        val store = BrowserStore(state)

        assertEquals("a", store.state.selectedTabId)

        store.dispatch(TabListAction.RemoveTabAction("a")).joinBlocking()

        assertNull(store.state.selectedTabId)
    }

    @Test
    fun `RemoveTabAction - Will select next nearby tab after removing selected tab`() {
        val state = BrowserState(
            tabs = listOf(
                createTab(id = "a", url = "https://www.mozilla.org"),
                createTab(id = "b", url = "https://www.firefox.com"),
                createTab(id = "c", url = "https://www.example.org"),
                createTab(id = "d", url = "https://getpocket.com")
            ),
            customTabs = listOf(
                createCustomTab(id = "a1", url = "https://www.firefox.com")
            ),
            selectedTabId = "c"
        )

        val store = BrowserStore(state)

        assertEquals("c", store.state.selectedTabId)

        store.dispatch(TabListAction.RemoveTabAction("c")).joinBlocking()
        assertEquals("d", store.state.selectedTabId)

        store.dispatch(TabListAction.RemoveTabAction("a")).joinBlocking()
        assertEquals("d", store.state.selectedTabId)

        store.dispatch(TabListAction.RemoveTabAction("d")).joinBlocking()
        assertEquals("b", store.state.selectedTabId)

        store.dispatch(TabListAction.RemoveTabAction("b")).joinBlocking()
        assertNull(store.state.selectedTabId)
    }

    @Test
    fun `RemoveTabAction - Selects private tab after private tab was removed`() {
        val state = BrowserState(
            tabs = listOf(
                createTab(id = "a", url = "https://www.mozilla.org", private = true),
                createTab(id = "b", url = "https://www.firefox.com", private = false),
                createTab(id = "c", url = "https://www.example.org", private = false),
                createTab(id = "d", url = "https://getpocket.com", private = true),
                createTab(id = "e", url = "https://developer.mozilla.org/", private = true)
            ),
            customTabs = listOf(
                createCustomTab(id = "a1", url = "https://www.firefox.com"),
                createCustomTab(id = "b1", url = "https://hubs.mozilla.com")
            ),
            selectedTabId = "d"
        )

        val store = BrowserStore(state)

        // [a*, b, c, (d*), e*] -> [a*, b, c, (e*)]
        store.dispatch(TabListAction.RemoveTabAction("d")).joinBlocking()
        assertEquals("e", store.state.selectedTabId)

        // [a*, b, c, (e*)] -> [(a*), b, c]
        store.dispatch(TabListAction.RemoveTabAction("e")).joinBlocking()
        assertEquals("a", store.state.selectedTabId)

        // After removing the last private tab a normal tab will be selected
        // [(a*), b, c] -> [b, (c)]
        store.dispatch(TabListAction.RemoveTabAction("a")).joinBlocking()
        assertEquals("c", store.state.selectedTabId)
    }

    @Test
    fun `RemoveTabAction - Selects normal tab after normal tab was removed`() {
        val state = BrowserState(
            tabs = listOf(
                createTab(id = "a", url = "https://www.mozilla.org", private = false),
                createTab(id = "b", url = "https://www.firefox.com", private = true),
                createTab(id = "c", url = "https://www.example.org", private = true),
                createTab(id = "d", url = "https://getpocket.com", private = false),
                createTab(id = "e", url = "https://developer.mozilla.org/", private = false)
            ),
            customTabs = listOf(
                createCustomTab(id = "a1", url = "https://www.firefox.com"),
                createCustomTab(id = "b1", url = "https://hubs.mozilla.com")
            ),
            selectedTabId = "d"
        )

        val store = BrowserStore(state)

        // [a, b*, c*, (d), e] -> [a, b*, c* (e)]
        store.dispatch(TabListAction.RemoveTabAction("d")).joinBlocking()
        assertEquals("e", store.state.selectedTabId)

        // [a, b*, c*, (e)] -> [(a), b*, c*]
        store.dispatch(TabListAction.RemoveTabAction("e")).joinBlocking()
        assertEquals("a", store.state.selectedTabId)

        // After removing the last normal tab NO private tab should get selected
        // [(a), b*, c*] -> [b*, c*]
        store.dispatch(TabListAction.RemoveTabAction("a")).joinBlocking()
        assertNull(store.state.selectedTabId)
    }
}
