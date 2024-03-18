/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.crashes

import androidx.navigation.NavController
import io.mockk.Called
import io.mockk.every
import io.mockk.mockk
import io.mockk.spyk
import io.mockk.verify
import mozilla.components.lib.crash.Crash.NativeCodeCrash
import mozilla.components.support.test.ext.joinBlocking
import org.junit.Test
import org.mozilla.fenix.browser.BrowserFragmentDirections
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.Components
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.components.appstate.AppState
import org.mozilla.fenix.utils.Settings

class CrashReporterControllerTest {

    private val sessionId = "testId"
    private val components: Components = mockk(relaxed = true)
    private val settings: Settings = mockk(relaxed = true)
    private val navController: NavController = mockk(relaxed = true)
    private val crash: NativeCodeCrash = mockk(relaxed = true)
    private var appStore = AppStore(
        AppState(
            nonFatalCrashes = listOf(crash),
        ),
    )
    private var controller = CrashReporterController(sessionId, 2, components, settings, navController, appStore)

    @Test
    fun `GIVEN reportCrashes true WHEN user restores tab THEN try submitting non-fatal crashes and recover tabs`() {
        controller = spyk(controller)

        controller.handleCloseAndRestore(true)

        verify { controller.submitPendingNonFatalCrashesIfNecessary(true) }
        verify { components.useCases.sessionUseCases.crashRecovery.invoke() }
    }

    @Test
    fun `GIVEN reportCrashes false WHEN user restores tab THEN try submitting non-fatal crashes and recover tabs`() {
        controller = spyk(controller)

        controller.handleCloseAndRestore(false)

        verify { controller.submitPendingNonFatalCrashesIfNecessary(false) }
        verify { components.useCases.sessionUseCases.crashRecovery.invoke() }
    }

    @Test
    fun `GIVEN reportCrashes true WHEN user closes the tab THEN try submitting non-fatal crashes, remove the current tab and recover others`() {
        controller = spyk(controller)

        controller.handleCloseAndRemove(true)

        verify { controller.submitPendingNonFatalCrashesIfNecessary(true) }
        verify { components.useCases.tabsUseCases.removeTab(sessionId) }
        verify { components.useCases.sessionUseCases.crashRecovery.invoke() }
    }

    @Test
    fun `GIVEN reportCrashes false WHEN user closes the tab THEN try submitting non-fatal crashes, remove the current tab and recover others`() {
        controller = spyk(controller)

        controller.handleCloseAndRemove(false)

        verify { controller.submitPendingNonFatalCrashesIfNecessary(false) }
        verify { components.useCases.tabsUseCases.removeTab(sessionId) }
        verify { components.useCases.sessionUseCases.crashRecovery.invoke() }
    }

    @Test
    fun `GIVEN reportCrashes false WHEN trying to submit crashes THEN no crashes should be submitted and all should be disposed off`() {
        val enabledCrashReporterSettings: Settings = mockk {
            every { isCrashReportingEnabled } returns true
        }
        appStore = spyk(appStore)
        controller = CrashReporterController(sessionId, 2, components, enabledCrashReporterSettings, navController, appStore)

        controller.submitPendingNonFatalCrashesIfNecessary(false)?.joinBlocking()

        verify(exactly = 0) { components.analytics.crashReporter.submitReport(crash) }
        verify { appStore.dispatch(AppAction.RemoveAllNonFatalCrashes) }
    }

    @Test
    fun `GIVEN reportCrashes true but reporting crashes disabled WHEN trying to submit crashes THEN no crashes should be submitted and all should be disposed off`() {
        val disabledCrashReporterSettings: Settings = mockk {
            every { isCrashReportingEnabled } returns false
        }
        appStore = spyk(appStore)
        controller = CrashReporterController(sessionId, 2, components, disabledCrashReporterSettings, navController, appStore)

        controller.submitPendingNonFatalCrashesIfNecessary(true)?.joinBlocking()

        verify(exactly = 0) { components.analytics.crashReporter.submitReport(crash) }
        verify { appStore.dispatch(AppAction.RemoveAllNonFatalCrashes) }
    }

    @Test
    fun `GIVEN reportCrashes true and reporting crashes enabled WHEN trying to submit crashes THEN all crashes should be submitted and then disposed off`() {
        val disabledCrashReporterSettings: Settings = mockk {
            every { isCrashReportingEnabled } returns true
        }
        appStore = spyk(appStore)
        controller = CrashReporterController(sessionId, 2, components, disabledCrashReporterSettings, navController, appStore)

        controller.submitPendingNonFatalCrashesIfNecessary(true)!!.joinBlocking()

        verify { components.analytics.crashReporter.submitReport(crash) }
        verify { appStore.dispatch(AppAction.RemoveNonFatalCrash(crash)) }
    }

    @Test
    fun `GIVEN only one tab opened WHEN user closes the tab THEN navigate to Home`() {
        controller = CrashReporterController(sessionId, 1, components, settings, navController, appStore)

        controller.handleCloseAndRemove(true)

        verify { navController.navigate(BrowserFragmentDirections.actionGlobalHome()) }
    }

    @Test
    fun `GIVEN multiple tabs opened WHEN user closes one tab THEN don't use navigation`() {
        controller = CrashReporterController(sessionId, 2, components, settings, navController, appStore)

        controller.handleCloseAndRemove(true)

        verify { navController wasNot Called }
    }
}
