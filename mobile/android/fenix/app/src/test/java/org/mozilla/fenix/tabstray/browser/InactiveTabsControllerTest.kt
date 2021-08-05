/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import io.mockk.mockk
import io.mockk.slot
import io.mockk.verify
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.concept.tabstray.Tabs
import mozilla.components.concept.tabstray.TabsTray
import org.junit.Assert.assertEquals
import mozilla.components.browser.state.state.createTab as createTabState
import org.junit.Test

class InactiveTabsControllerTest {
    @Test
    fun `WHEN expanded THEN notify filtered card`() {
        val filter: (TabSessionState) -> Boolean = { !it.content.private }
        val store = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTabState("https://mozilla.org", id = "1"),
                    createTabState("https://firefox.com", id = "2"),
                    createTabState("https://getpocket.com", id = "3", private = true)
                )
            )
        )
        val tray: TabsTray = mockk(relaxed = true)
        val tabsSlot = slot<Tabs>()
        val controller = InactiveTabsController(store, filter, tray)

        controller.updateCardExpansion(true)

        verify { tray.updateTabs(capture(tabsSlot)) }

        assertEquals(2, tabsSlot.captured.list.size)
        assertEquals("1", tabsSlot.captured.list.first().id)
    }
}
