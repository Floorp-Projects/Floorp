/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.crashes

import android.view.ViewGroup.MarginLayoutParams
import io.mockk.every
import io.mockk.mockk
import io.mockk.slot
import io.mockk.verify
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.browser.state.action.CrashAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.state.createTab
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.toolbar.BrowserToolbar
import mozilla.components.support.test.libstate.ext.waitUntilIdle
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.Components
import org.mozilla.fenix.utils.Settings

class CrashContentIntegrationTest {
    @OptIn(ExperimentalCoroutinesApi::class)
    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()

    private val sessionId = "sessionId"
    private lateinit var browserStore: BrowserStore

    @Before
    fun setup() {
        browserStore = BrowserStore(
            BrowserState(
                tabs = listOf(
                    createTab("url", id = sessionId)
                ),
                selectedTabId = sessionId
            )
        )
    }

    @Test
    fun `GIVEN a tab WHEN its content crashes THEN expand the toolbar and show the in-content crash reporter`() {
        val crashReporterLayoutParams: MarginLayoutParams = mockk(relaxed = true)
        val crashReporterView: CrashReporterFragment = mockk(relaxed = true) {
            every { layoutParams } returns crashReporterLayoutParams
        }
        val toolbar: BrowserToolbar = mockk(relaxed = true) {
            every { height } returns 33
        }
        val components: Components = mockk()
        val settings: Settings = mockk()
        val appStore: AppStore = mockk()
        val integration = CrashContentIntegration(
            browserStore = browserStore,
            appStore = appStore,
            toolbar = toolbar,
            isToolbarPlacedAtTop = true,
            crashReporterView = crashReporterView,
            components = components,
            settings = settings,
            navController = mockk(),
            sessionId = sessionId
        )
        val controllerCaptor = slot<CrashReporterController>()
        integration.start()
        browserStore.dispatch(CrashAction.SessionCrashedAction(sessionId))
        browserStore.waitUntilIdle()

        verify {
            toolbar.expand()
            crashReporterLayoutParams.topMargin = 33
            crashReporterView.show(capture(controllerCaptor))
        }
        assertEquals(sessionId, controllerCaptor.captured.sessionId)
        assertEquals(components, controllerCaptor.captured.components)
        assertEquals(settings, controllerCaptor.captured.settings)
        assertEquals(appStore, controllerCaptor.captured.appStore)
    }

    @Test
    fun `GIVEN a tab is marked as crashed WHEN the crashed state changes THEN hide the in-content crash reporter`() {
        val crashReporterView: CrashReporterFragment = mockk(relaxed = true)
        val integration = CrashContentIntegration(
            browserStore = browserStore,
            appStore = mockk(),
            toolbar = mockk(),
            isToolbarPlacedAtTop = true,
            crashReporterView = crashReporterView,
            components = mockk(),
            settings = mockk(),
            navController = mockk(),
            sessionId = sessionId,
        )

        integration.start()
        browserStore.dispatch(CrashAction.RestoreCrashedSessionAction(sessionId))
        browserStore.waitUntilIdle()

        verify { crashReporterView.hide() }
    }
}
