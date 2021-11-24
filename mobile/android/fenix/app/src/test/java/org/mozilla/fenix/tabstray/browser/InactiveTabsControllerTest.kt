/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.slot
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.tabstray.TabsTray
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import org.junit.Assert.assertEquals
import mozilla.components.browser.state.state.createTab as createTabState
import org.junit.Test
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.tabstray.TabsTrayState
import org.mozilla.fenix.tabstray.TabsTrayStore
import org.mozilla.fenix.utils.Settings

class InactiveTabsControllerTest {

    private val metrics: MetricController = mockk(relaxed = true)
    private val settings: Settings = mockk(relaxed = true)
    private val appStore = AppStore()

    @Test
    fun `WHEN expanded THEN notify filtered card`() {
        val store = TabsTrayStore(
            TabsTrayState(
                inactiveTabs = listOf(
                    createTabState("https://mozilla.org", id = "1"),
                    createTabState("https://firefox.com", id = "2")
                )
            )
        )
        val tray: TabsTray = mockk(relaxed = true)
        val tabsSlot = slot<List<TabSessionState>>()
        val controller =
            InactiveTabsController(store, appStore, tray, mockk(relaxed = true), settings)

        controller.updateCardExpansion(true)

        appStore.waitUntilIdle()

        verify { tray.updateTabs(capture(tabsSlot), any()) }

        assertEquals(2, tabsSlot.captured.size)
        assertEquals("1", tabsSlot.captured.first().id)
    }

    @Test
    fun `WHEN expanded THEN track telemetry event`() {
        val store = TabsTrayStore()
        val controller = InactiveTabsController(
            store, appStore, mockk(relaxed = true), metrics, settings
        )

        controller.updateCardExpansion(true)

        verify { metrics.track(Event.TabsTrayInactiveTabsExpanded) }
    }

    @Test
    fun `WHEN collapsed THEN track telemetry event`() {
        val store = TabsTrayStore()
        val controller = InactiveTabsController(
            store, appStore, mockk(relaxed = true), metrics, settings
        )

        controller.updateCardExpansion(false)

        verify { metrics.track(Event.TabsTrayInactiveTabsCollapsed) }
    }

    @Test
    fun `WHEN close THEN update settings and refresh`() {
        val store = TabsTrayStore()
        val controller = spyk(
            InactiveTabsController(
                store, appStore, mockk(relaxed = true), metrics, settings
            )
        )

        every { controller.refreshInactiveTabsSection() } just Runs

        controller.close()

        verify { metrics.track(Event.TabsTrayAutoCloseDialogDismissed) }
        verify { settings.hasInactiveTabsAutoCloseDialogBeenDismissed = true }
        verify { controller.refreshInactiveTabsSection() }
    }

    @Test
    fun `WHEN enableAutoClosed THEN update closeTabsAfterOneMonth settings and refresh`() {
        val filter: (TabSessionState) -> Boolean = { !it.content.private }
        val store = BrowserStore()
        val tray: TabsTray = mockk(relaxed = true)
        val controller =
            spyk(InactiveTabsAutoCloseDialogController(store, settings, filter, tray, metrics))

        every { controller.refreshInactiveTabsSection() } just Runs

        controller.enableAutoClosed()

        verify { metrics.track(Event.TabsTrayAutoCloseDialogTurnOnClicked) }
        verify { settings.closeTabsAfterOneMonth = true }
        verify { settings.closeTabsAfterOneWeek = false }
        verify { settings.closeTabsAfterOneDay = false }
        verify { settings.manuallyCloseTabs = false }
        verify { controller.refreshInactiveTabsSection() }
    }
}
