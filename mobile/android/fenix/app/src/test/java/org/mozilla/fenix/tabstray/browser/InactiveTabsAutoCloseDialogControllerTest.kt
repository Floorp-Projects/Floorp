/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.browser

import io.mockk.Runs
import io.mockk.every
import io.mockk.just
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.tabstray.TabsTray
import org.junit.Test
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.utils.Settings

class InactiveTabsAutoCloseDialogControllerTest {

    val metrics: MetricController = mockk(relaxed = true)

    @Test
    fun `WHEN close THEN update settings and refresh`() {
        val filter: (TabSessionState) -> Boolean = { !it.content.private }
        val store = BrowserStore()
        val settings: Settings = mockk(relaxed = true)
        val tray: TabsTray = mockk(relaxed = true)
        val controller = spyk(InactiveTabsAutoCloseDialogController(store, settings, filter, tray, metrics))

        every { controller.refeshInactiveTabsSecion() } just Runs

        controller.close()

        verify { metrics.track(Event.TabsTrayAutoCloseDialogDismissed) }
        verify { settings.hasInactiveTabsAutoCloseDialogBeenDismissed = true }
        verify { controller.refeshInactiveTabsSecion() }
    }

    @Test
    fun `WHEN enableAutoClosed THEN update closeTabsAfterOneMonth settings and refresh`() {
        val filter: (TabSessionState) -> Boolean = { !it.content.private }
        val store = BrowserStore()
        val settings: Settings = mockk(relaxed = true)
        val tray: TabsTray = mockk(relaxed = true)
        val controller = spyk(InactiveTabsAutoCloseDialogController(store, settings, filter, tray, metrics))

        every { controller.refeshInactiveTabsSecion() } just Runs

        controller.enableAutoClosed()

        verify { metrics.track(Event.TabsTrayAutoCloseDialogTurnOnClicked) }
        verify { settings.closeTabsAfterOneMonth = true }
        verify { settings.closeTabsAfterOneWeek = false }
        verify { settings.closeTabsAfterOneDay = false }
        verify { settings.manuallyCloseTabs = false }
        verify { controller.refeshInactiveTabsSecion() }
    }
}
