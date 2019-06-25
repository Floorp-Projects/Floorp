/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.action

import kotlinx.coroutines.runBlocking
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.ext.joinBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class TabListActionTest {
    @Test
    fun `AddTabAction - Adds provided SessionState`() = runBlocking {
        val state = BrowserState()
        val store = BrowserStore(state)

        assertEquals(0, store.state.tabs.size)
        assertNull(store.state.selectedTabId)

        val tab = createTab(url = "https://www.mozilla.org")

        store.dispatch(TabListAction.AddTabAction(tab))
            .join()

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
    fun `SelectTabAction - Selects SessionState by id`() = runBlocking {
        val state = BrowserState(
            tabs = listOf(
                createTab(id = "a", url = "https://www.mozilla.org"),
                createTab(id = "b", url = "https://www.firefox.com")
            )
        )
        val store = BrowserStore(state)

        assertNull(store.state.selectedTabId)

        store.dispatch(TabListAction.SelectTabAction("a"))
            .join()

        assertEquals("a", store.state.selectedTabId)
    }

    @Test
    fun `RemoveTabAction - Removes SessionState`() = runBlocking {
        val state = BrowserState(
            tabs = listOf(
                createTab(id = "a", url = "https://www.mozilla.org"),
                createTab(id = "b", url = "https://www.firefox.com")
            )
        )
        val store = BrowserStore(state)

        store.dispatch(TabListAction.RemoveTabAction("a"))
            .join()

        assertEquals(1, store.state.tabs.size)
        assertEquals("https://www.firefox.com", store.state.tabs[0].content.url)
    }

    @Test
    fun `RemoveTabAction - Noop for unknown id`() = runBlocking {
        val state = BrowserState(
            tabs = listOf(
                createTab(id = "a", url = "https://www.mozilla.org"),
                createTab(id = "b", url = "https://www.firefox.com")
            )
        )
        val store = BrowserStore(state)

        store.dispatch(TabListAction.RemoveTabAction("c"))
            .join()

        assertEquals(2, store.state.tabs.size)
        assertEquals("https://www.mozilla.org", store.state.tabs[0].content.url)
        assertEquals("https://www.firefox.com", store.state.tabs[1].content.url)
    }
}