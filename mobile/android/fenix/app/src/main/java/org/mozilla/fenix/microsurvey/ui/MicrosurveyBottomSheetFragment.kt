/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.microsurvey.ui

import android.app.Dialog
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.platform.ViewCompositionStrategy
import com.google.android.material.bottomsheet.BottomSheetBehavior
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import org.mozilla.fenix.R
import org.mozilla.fenix.theme.FirefoxTheme

/**
 * todo update behaviour FXDROID-1944.
 * todo pass question and icon values from messaging FXDROID-1945.
 * todo add dismiss request FXDROID-1946.
 */

/**
 * A bottom sheet fragment for displaying a microsurvey.
 */
class MicrosurveyBottomSheetFragment : BottomSheetDialogFragment() {

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog =
        super.onCreateDialog(savedInstanceState).apply {
            setOnShowListener {
                val bottomSheet = findViewById<View?>(R.id.design_bottom_sheet)
                bottomSheet?.setBackgroundResource(android.R.color.transparent)
                val behavior = BottomSheetBehavior.from(bottomSheet)
                behavior.setPeekHeightToHalfScreenHeight()
                behavior.state = BottomSheetBehavior.STATE_HALF_EXPANDED
            }
        }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View = ComposeView(requireContext()).apply {
        val answers = listOf(
            getString(R.string.likert_scale_option_1),
            getString(R.string.likert_scale_option_2),
            getString(R.string.likert_scale_option_3),
            getString(R.string.likert_scale_option_4),
            getString(R.string.likert_scale_option_5),
        )

        setViewCompositionStrategy(ViewCompositionStrategy.DisposeOnViewTreeLifecycleDestroyed)

        setContent {
            FirefoxTheme {
                MicrosurveyBottomSheet(
                    question = "How satisfied are you with printing in Firefox?", // todo get value from messaging
                    icon = R.drawable.ic_print, // todo get value from messaging
                    answers = answers, // todo get value from messaging
                )
            }
        }
    }

    private fun BottomSheetBehavior<View>.setPeekHeightToHalfScreenHeight() {
        peekHeight = resources.displayMetrics.heightPixels / 2
    }
}
