/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.crashes

import androidx.navigation.NavController
import mozilla.components.browser.session.Session
import mozilla.components.lib.crash.Crash
import org.mozilla.fenix.R
import org.mozilla.fenix.components.Components
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.utils.Settings

class CrashReporterController(
    private val crash: Crash,
    private val session: Session?,
    private val navController: NavController,
    private val components: Components,
    private val settings: Settings
) {

    init {
        components.analytics.metrics.track(Event.CrashReporterOpened)
    }

    /**
     * Closes the crash reporter fragment and tries to recover the session.
     *
     * @param sendCrash If true, submit a crash report.
     */
    fun handleCloseAndRestore(sendCrash: Boolean) {
        submitReportIfNecessary(sendCrash)

        components.useCases.sessionUseCases.crashRecovery.invoke()
        navController.popBackStack()
    }

    /**
     * Closes the crash reporter fragment and the tab.
     *
     * @param sendCrash If true, submit a crash report.
     */
    fun handleCloseAndRemove(sendCrash: Boolean) {
        session ?: return
        submitReportIfNecessary(sendCrash)

        components.useCases.tabsUseCases.removeTab(session)
        components.useCases.sessionUseCases.crashRecovery.invoke()
        navController.nav(
            R.id.crashReporterFragment,
            CrashReporterFragmentDirections.actionCrashReporterFragmentToHomeFragment()
        )
    }

    /**
     * Submits the crash report if the "Send crash" checkbox was checked and the setting is enabled.
     */
    private fun submitReportIfNecessary(sendCrash: Boolean) {
        val didSubmitReport = if (sendCrash && settings.isCrashReportingEnabled) {
            components.analytics.crashReporter.submitReport(crash)
            true
        } else {
            false
        }

        components.analytics.metrics.track(Event.CrashReporterClosed(didSubmitReport))
    }
}
