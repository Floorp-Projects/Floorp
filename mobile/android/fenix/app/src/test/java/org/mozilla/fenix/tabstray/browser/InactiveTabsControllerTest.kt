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
import mozilla.components.service.glean.testing.GleanTestRule
import org.mozilla.fenix.GleanMetrics.TabsTray as TabsTrayMetrics
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import mozilla.components.browser.state.state.createTab as createTabState
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.tabstray.TabsTrayState
import org.mozilla.fenix.tabstray.TabsTrayStore
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class) // for gleanTestRule
class InactiveTabsControllerTest {

    private val metrics: MetricController = mockk(relaxed = true)
    private val settings: Settings = mockk(relaxed = true)
    private val appStore = AppStore()

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

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
            InactiveTabsController(store, appStore, tray, settings)

        controller.updateCardExpansion(true)

        appStore.waitUntilIdle()

        verify { tray.updateTabs(capture(tabsSlot), null, any()) }

        assertEquals(2, tabsSlot.captured.size)
        assertEquals("1", tabsSlot.captured.first().id)
    }

    @Test
    fun `WHEN expanded THEN track telemetry event`() {
        val store = TabsTrayStore()
        val controller = InactiveTabsController(
            store, appStore, mockk(relaxed = true), settings
        )

        assertFalse(TabsTrayMetrics.inactiveTabsExpanded.testHasValue())
        assertFalse(TabsTrayMetrics.inactiveTabsCollapsed.testHasValue())

        controller.updateCardExpansion(true)

        assertTrue(TabsTrayMetrics.inactiveTabsExpanded.testHasValue())
        assertFalse(TabsTrayMetrics.inactiveTabsCollapsed.testHasValue())
    }

    @Test
    fun `WHEN collapsed THEN track telemetry event`() {
        val store = TabsTrayStore()
        val controller = InactiveTabsController(
            store, appStore, mockk(relaxed = true), settings
        )

        assertFalse(TabsTrayMetrics.inactiveTabsExpanded.testHasValue())
        assertFalse(TabsTrayMetrics.inactiveTabsCollapsed.testHasValue())

        controller.updateCardExpansion(false)

        assertFalse(TabsTrayMetrics.inactiveTabsExpanded.testHasValue())
        assertTrue(TabsTrayMetrics.inactiveTabsCollapsed.testHasValue())
    }

    @Test
    fun `WHEN close THEN update settings and refresh`() {
        val store = TabsTrayStore()
        val controller = spyk(
            InactiveTabsController(
                store, appStore, mockk(relaxed = true), settings
            )
        )

        every { controller.refreshInactiveTabsSection() } just Runs

        assertFalse(TabsTrayMetrics.autoCloseDimissed.testHasValue())

        controller.close()

        assertTrue(TabsTrayMetrics.autoCloseDimissed.testHasValue())
        verify { settings.hasInactiveTabsAutoCloseDialogBeenDismissed = true }
        verify { controller.refreshInactiveTabsSection() }
    }

    @Test
    fun `WHEN enableAutoClosed THEN update closeTabsAfterOneMonth settings and refresh`() {
        val filter: (TabSessionState) -> Boolean = { !it.content.private }
        val store = BrowserStore()
        val tray: TabsTray = mockk(relaxed = true)
        val controller =
            spyk(InactiveTabsAutoCloseDialogController(store, settings, filter, tray))

        every { controller.refreshInactiveTabsSection() } just Runs

        assertFalse(TabsTrayMetrics.autoCloseTurnOnClicked.testHasValue())

        controller.enableAutoClosed()

        assertTrue(TabsTrayMetrics.autoCloseTurnOnClicked.testHasValue())

        verify { settings.closeTabsAfterOneMonth = true }
        verify { settings.closeTabsAfterOneWeek = false }
        verify { settings.closeTabsAfterOneDay = false }
        verify { settings.manuallyCloseTabs = false }
        verify { controller.refreshInactiveTabsSection() }
    }
}
