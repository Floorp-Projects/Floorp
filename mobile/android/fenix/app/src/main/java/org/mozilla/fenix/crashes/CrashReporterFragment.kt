/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.crashes

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import androidx.navigation.Navigation
import kotlinx.android.synthetic.main.fragment_crash_reporter.*
import mozilla.components.lib.crash.Crash
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.requireComponents

class CrashReporterFragment : Fragment() {
    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? = inflater.inflate(R.layout.fragment_crash_reporter, container, false)

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        val crashIntent = CrashReporterFragmentArgs.fromBundle(arguments!!).crashIntent

        val crash = Crash.fromIntent(CrashReporterFragmentArgs.fromBundle(arguments!!).crashIntent)
        // TODO TelemetryWrapper.crashReporterOpened()

        closeTabButton.setOnClickListener {
            val wantsToSubmitCrashReport = sendCrashCheckbox.isChecked
            val selectedSession = requireComponents.core.sessionManager.selectedSession

            selectedSession?.let { session -> requireComponents.useCases.tabsUseCases.removeTab.invoke(session) }
            // TODO TelemetryWrapper.crashReporterClosed(wantsSubmitCrashReport)

            if (wantsToSubmitCrashReport) {
                requireComponents.analytics.crashReporter.submitReport(crash)
            }

            navigateHome(view)
        }
    }

    fun navigateHome(fromView: View) {
        val directions = CrashReporterFragmentDirections.actionCrashReporterFragmentToHomeFragment()
        Navigation.findNavController(fromView).navigate(directions)
    }

    fun onBackPressed() {
        // TODO TelemetryWrapper.crashReporterClosed(false)
    }
}
