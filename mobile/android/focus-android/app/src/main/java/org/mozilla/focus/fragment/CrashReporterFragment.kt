/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.fragment

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.fragment.app.Fragment
import kotlinx.android.synthetic.main.fragment_crash_reporter.*
import org.mozilla.focus.R
import org.mozilla.focus.telemetry.TelemetryWrapper

class CrashReporterFragment : Fragment() {
    var onCloseTabPressed: ((sendCrashReport: Boolean) -> Unit)? = null

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? = inflater.inflate(R.layout.fragment_crash_reporter, container, false)

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        TelemetryWrapper.crashReporterOpened()

        closeTabButton.setOnClickListener {
            val wantsSubmitCrashReport = sendCrashCheckbox.isChecked
            TelemetryWrapper.closeTabButtonTapped(wantsSubmitCrashReport)

            onCloseTabPressed?.invoke(wantsSubmitCrashReport)
        }
    }

    companion object {
        val FRAGMENT_TAG = "crash-reporter"

        fun create() = CrashReporterFragment()
    }
}
