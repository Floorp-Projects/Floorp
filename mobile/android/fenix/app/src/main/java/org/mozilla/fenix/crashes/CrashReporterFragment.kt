/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.crashes

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.Fragment
import androidx.navigation.fragment.findNavController
import androidx.navigation.fragment.navArgs
import kotlinx.android.synthetic.main.fragment_crash_reporter.*
import mozilla.components.lib.crash.Crash
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.requireComponents
import org.mozilla.fenix.ext.settings

/**
 * Fragment shown when a tab crashes.
 */
class CrashReporterFragment : Fragment() {

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? = inflater.inflate(R.layout.fragment_crash_reporter, container, false)

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        val args: CrashReporterFragmentArgs by navArgs()
        val crash = Crash.fromIntent(args.crashIntent)

        title.text = getString(R.string.tab_crash_title_2, getString(R.string.app_name))

        val controller = CrashReporterController(
            crash,
            session = requireComponents.core.sessionManager.selectedSession,
            navController = findNavController(),
            components = requireComponents,
            settings = requireContext().settings
        )

        restoreTabButton.setOnClickListener {
            controller.handleCloseAndRestore(sendCrashCheckbox.isChecked)
        }
        closeTabButton.setOnClickListener {
            controller.handleCloseAndRemove(sendCrashCheckbox.isChecked)
        }
    }

    override fun onResume() {
        super.onResume()
        (activity as AppCompatActivity).supportActionBar?.hide()
    }
}
