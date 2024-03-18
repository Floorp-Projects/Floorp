/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.action

import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.CustomTabConfig
import mozilla.components.browser.state.state.SessionState
import mozilla.components.browser.state.state.createCustomTab
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.support.test.ext.joinBlocking
import org.junit.Assert.assertEquals
import org.junit.Assert.assertSame
import org.junit.Test

class CustomTabListActionTest {

    @Test
    fun `AddCustomTabAction - Adds provided tab`() {
        val store = BrowserStore()

        assertEquals(0, store.state.tabs.size)
        assertEquals(0, store.state.customTabs.size)

        val config = CustomTabConfig()
        val customTab = createCustomTab(
            "https://www.mozilla.org",
            config = config,
            source = SessionState.Source.Internal.CustomTab,
        )

        store.dispatch(CustomTabListAction.AddCustomTabAction(customTab)).joinBlocking()

        assertEquals(0, store.state.tabs.size)
        assertEquals(1, store.state.customTabs.size)
        assertEquals(SessionState.Source.Internal.CustomTab, store.state.customTabs[0].source)
        assertEquals(customTab, store.state.customTabs[0])
        assertSame(config, store.state.customTabs[0].config)
    }

    @Test
    fun `RemoveCustomTabAction - Removes tab with given id`() {
        val customTab1 = createCustomTab("https://www.mozilla.org")
        val customTab2 = createCustomTab("https://www.firefox.com")

        val state = BrowserState(customTabs = listOf(customTab1, customTab2))
        val store = BrowserStore(state)

        assertEquals(2, store.state.customTabs.size)

        store.dispatch(CustomTabListAction.RemoveCustomTabAction(customTab2.id)).joinBlocking()

        assertEquals(1, store.state.customTabs.size)
        assertEquals(customTab1, store.state.customTabs[0])
    }

    @Test
    fun `RemoveCustomTabAction - Noop for unknown id`() {
        val customTab1 = createCustomTab("https://www.mozilla.org")
        val customTab2 = createCustomTab("https://www.firefox.com")

        val state = BrowserState(customTabs = listOf(customTab1, customTab2))
        val store = BrowserStore(state)

        assertEquals(2, store.state.customTabs.size)

        store.dispatch(CustomTabListAction.RemoveCustomTabAction("unknown id")).joinBlocking()

        assertEquals(2, store.state.customTabs.size)
        assertEquals(customTab1, store.state.customTabs[0])
        assertEquals(customTab2, store.state.customTabs[1])
    }

    @Test
    fun `RemoveAllCustomTabsAction - Removes all custom tabs (but not regular tabs)`() {
        val customTab1 = createCustomTab("https://www.mozilla.org")
        val customTab2 = createCustomTab("https://www.firefox.com")
        val regularTab = createTab(url = "https://www.mozilla.org")

        val state = BrowserState(customTabs = listOf(customTab1, customTab2), tabs = listOf(regularTab))
        val store = BrowserStore(state)

        assertEquals(2, store.state.customTabs.size)
        assertEquals(1, store.state.tabs.size)

        store.dispatch(CustomTabListAction.RemoveAllCustomTabsAction).joinBlocking()
        assertEquals(0, store.state.customTabs.size)
        assertEquals(1, store.state.tabs.size)
    }
}
