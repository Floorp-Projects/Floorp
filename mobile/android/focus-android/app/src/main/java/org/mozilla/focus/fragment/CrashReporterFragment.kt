/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.fragment

import android.os.Bundle
import android.view.View
import androidx.appcompat.content.res.AppCompatResources
import androidx.fragment.app.Fragment
import mozilla.components.service.glean.private.NoExtras
import org.mozilla.focus.GleanMetrics.CrashReporter
import org.mozilla.focus.R
import org.mozilla.focus.databinding.FragmentCrashReporterBinding

class CrashReporterFragment : Fragment(R.layout.fragment_crash_reporter) {
    var onCloseTabPressed: ((sendCrashReport: Boolean) -> Unit)? = null

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        val binding = FragmentCrashReporterBinding.bind(view)
        binding.background.background =
            AppCompatResources.getDrawable(requireContext(), R.drawable.ic_error_session_crashed)

        CrashReporter.displayed.record(NoExtras())

        binding.closeTabButton.setOnClickListener {
            val wantsSubmitCrashReport = binding.sendCrashCheckbox.isChecked
            CrashReporter.closeReport.record(CrashReporter.CloseReportExtra(wantsSubmitCrashReport))

            onCloseTabPressed?.invoke(wantsSubmitCrashReport)
        }
    }

    companion object {
        val FRAGMENT_TAG = "crash-reporter"

        fun create() = CrashReporterFragment()
    }
}
