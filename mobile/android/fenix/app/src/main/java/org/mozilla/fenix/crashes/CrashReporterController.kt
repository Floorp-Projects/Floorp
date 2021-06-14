/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.crashes

import androidx.navigation.NavController
import kotlinx.coroutines.DelicateCoroutinesApi
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import mozilla.components.lib.crash.Crash
import org.mozilla.fenix.R
import org.mozilla.fenix.components.Components
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.nav
import org.mozilla.fenix.utils.Settings

class CrashReporterController(
    private val crash: Crash,
    private val sessionId: String?,
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
     * @return Job if report is submitted through an IO thread, null otherwise
     */
    fun handleCloseAndRestore(sendCrash: Boolean): Job? {
        val job = submitReportIfNecessary(sendCrash)

        components.useCases.sessionUseCases.crashRecovery.invoke()
        navController.popBackStack()
        return job
    }

    /**
     * Closes the crash reporter fragment and the tab.
     *
     * @param sendCrash If true, submit a crash report.
     * @return Job if report is submitted through an IO thread, null otherwise
     */
    fun handleCloseAndRemove(sendCrash: Boolean): Job? {
        sessionId ?: return null
        val job = submitReportIfNecessary(sendCrash)

        components.useCases.tabsUseCases.removeTab(sessionId)
        components.useCases.sessionUseCases.crashRecovery.invoke()

        navController.nav(
            R.id.crashReporterFragment,
            CrashReporterFragmentDirections.actionGlobalHome()
        )

        return job
    }

    /**
     * Submits the crash report if the "Send crash" checkbox was checked and the setting is enabled.
     *
     * @param sendCrash If true, submit a crash report.
     * @return Job if report is submitted through an IO thread, null otherwise
     */
    @OptIn(DelicateCoroutinesApi::class) // GlobalScope usage
    private fun submitReportIfNecessary(sendCrash: Boolean): Job? {
        var job: Job? = null
        val didSubmitReport = if (sendCrash && settings.isCrashReportingEnabled) {
            job = GlobalScope.launch(Dispatchers.IO) {
                components.analytics.crashReporter.submitReport(crash)
            }
            true
        } else {
            false
        }

        components.analytics.metrics.track(Event.CrashReporterClosed(didSubmitReport))
        return job
    }
}
