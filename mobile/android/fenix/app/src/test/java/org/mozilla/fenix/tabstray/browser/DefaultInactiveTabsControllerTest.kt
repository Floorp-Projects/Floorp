/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkStatic
import io.mockk.spyk
import io.mockk.unmockkStatic
import io.mockk.verify
import mozilla.components.browser.state.state.ContentState
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.feature.tabs.TabsUseCases
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.ext.maxActiveTime
import org.mozilla.fenix.ext.potentialInactiveTabs
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.utils.Settings
import org.mozilla.fenix.GleanMetrics.TabsTray as TabsTrayMetrics

@RunWith(FenixRobolectricTestRunner::class)
class DefaultInactiveTabsControllerTest {

    private val appStore: AppStore = mockk(relaxed = true)
    private val settings: Settings = mockk(relaxed = true)
    private val browserStore: BrowserStore = mockk(relaxed = true)
    private val tabsUseCases: TabsUseCases = mockk(relaxed = true)

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    @Test
    fun `WHEN the inactive tabs section is expanded THEN the expanded telemetry event should be reported`() {
        val controller = createController()

        assertNull(TabsTrayMetrics.inactiveTabsExpanded.testGetValue())
        assertNull(TabsTrayMetrics.inactiveTabsCollapsed.testGetValue())

        controller.updateCardExpansion(isExpanded = true)

        assertNotNull(TabsTrayMetrics.inactiveTabsExpanded.testGetValue())
        assertNull(TabsTrayMetrics.inactiveTabsCollapsed.testGetValue())
    }

    @Test
    fun `WHEN the inactive tabs section is collapsed THEN the collapsed telemetry event should be reported`() {
        val controller = createController()

        assertNull(TabsTrayMetrics.inactiveTabsExpanded.testGetValue())
        assertNull(TabsTrayMetrics.inactiveTabsCollapsed.testGetValue())

        controller.updateCardExpansion(isExpanded = false)

        assertNull(TabsTrayMetrics.inactiveTabsExpanded.testGetValue())
        assertNotNull(TabsTrayMetrics.inactiveTabsCollapsed.testGetValue())
    }

    @Test
    fun `WHEN the inactive tabs auto-close feature prompt is dismissed THEN update settings and report the telemetry event`() {
        val controller = spyk(createController())

        assertNull(TabsTrayMetrics.autoCloseDimissed.testGetValue())

        controller.dismissAutoCloseDialog()

        assertNotNull(TabsTrayMetrics.autoCloseDimissed.testGetValue())
        verify { settings.hasInactiveTabsAutoCloseDialogBeenDismissed = true }
    }

    @Test
    fun `WHEN the inactive tabs auto-close feature prompt is accepted THEN update settings and report the telemetry event`() {
        val controller = spyk(createController())

        assertNull(TabsTrayMetrics.autoCloseTurnOnClicked.testGetValue())

        controller.enableInactiveTabsAutoClose()

        assertNotNull(TabsTrayMetrics.autoCloseTurnOnClicked.testGetValue())

        verify { settings.closeTabsAfterOneMonth = true }
        verify { settings.closeTabsAfterOneWeek = false }
        verify { settings.closeTabsAfterOneDay = false }
        verify { settings.manuallyCloseTabs = false }
        verify { settings.hasInactiveTabsAutoCloseDialogBeenDismissed = true }
    }

    @Test
    fun `WHEN an inactive tab is selected THEN report the telemetry event`() {
        val controller = createController()
        val tab = TabSessionState(
            id = "tabId",
            content = ContentState(
                url = "www.mozilla.com",
            )
        )

        assertNull(TabsTrayMetrics.openInactiveTab.testGetValue())

        controller.openInactiveTab(tab)

        assertNotNull(TabsTrayMetrics.openInactiveTab.testGetValue())
    }

    @Test
    fun `WHEN an inactive tab is closed THEN report the telemetry event`() {
        val controller = createController()
        val tab = TabSessionState(
            id = "tabId",
            content = ContentState(
                url = "www.mozilla.com",
            )
        )

        assertNull(TabsTrayMetrics.closeInactiveTab.testGetValue())

        controller.closeInactiveTab(tab)

        assertNotNull(TabsTrayMetrics.closeInactiveTab.testGetValue())
    }

    @Test
    fun `WHEN all inactive tabs are closed THEN perform the deletion and report the telemetry event and show a Snackbar`() {
        var showSnackbarInvoked = false
        val controller = createController(
            showUndoSnackbar = {
                showSnackbarInvoked = true
            }
        )
        val inactiveTab: TabSessionState = mockk {
            every { lastAccess } returns maxActiveTime
            every { createdAt } returns 0
            every { id } returns "24"
            every { content } returns mockk {
                every { private } returns false
            }
        }

        try {
            mockkStatic("mozilla.components.browser.state.selector.SelectorsKt")
            every { browserStore.state } returns mockk()
            every { browserStore.state.potentialInactiveTabs } returns listOf(inactiveTab)
            assertNull(TabsTrayMetrics.closeAllInactiveTabs.testGetValue())

            controller.deleteAllInactiveTabs()

            verify { tabsUseCases.removeTabs(listOf("24")) }
            assertNotNull(TabsTrayMetrics.closeAllInactiveTabs.testGetValue())
            assertTrue(showSnackbarInvoked)
        } finally {
            unmockkStatic("mozilla.components.browser.state.selector.SelectorsKt")
        }
    }

    private fun createController(
        showUndoSnackbar: (Boolean) -> Unit = { _ -> },
    ): DefaultInactiveTabsController {
        return DefaultInactiveTabsController(
            appStore = appStore,
            settings = settings,
            browserStore = browserStore,
            tabsUseCases = tabsUseCases,
            showUndoSnackbar = showUndoSnackbar,
        )
    }
}
