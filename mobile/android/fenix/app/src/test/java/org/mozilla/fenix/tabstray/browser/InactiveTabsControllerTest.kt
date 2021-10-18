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
import mozilla.components.browser.tabstray.TabsTray
import org.junit.Assert.assertEquals
import mozilla.components.browser.state.state.createTab as createTabState
import org.junit.Test
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController

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
        val tabsSlot = slot<List<TabSessionState>>()
        val controller = InactiveTabsController(store, filter, tray, mockk(relaxed = true))

        controller.updateCardExpansion(true)

        verify { tray.updateTabs(capture(tabsSlot), any()) }

        assertEquals(2, tabsSlot.captured.size)
        assertEquals("1", tabsSlot.captured.first().id)
    }

    @Test
    fun `WHEN expanded THEN track telemetry event`() {
        val metrics: MetricController = mockk(relaxed = true)
        val store = BrowserStore(BrowserState())
        val controller = InactiveTabsController(
            store, mockk(relaxed = true), mockk(relaxed = true), metrics
        )

        controller.updateCardExpansion(true)

        verify { metrics.track(Event.TabsTrayInactiveTabsExpanded) }
    }

    @Test
    fun `WHEN collapsed THEN track telemetry event`() {
        val metrics: MetricController = mockk(relaxed = true)
        val store = BrowserStore(BrowserState())
        val controller = InactiveTabsController(
            store, mockk(relaxed = true), mockk(relaxed = true), metrics
        )

        controller.updateCardExpansion(false)

        verify { metrics.track(Event.TabsTrayInactiveTabsCollapsed) }
    }
}
