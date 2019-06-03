/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.core.text.HtmlCompat
import androidx.fragment.app.DialogFragment
import androidx.navigation.fragment.NavHostFragment.findNavController
import com.google.android.material.bottomsheet.BottomSheetDialogFragment
import org.mozilla.fenix.R

class PairInstructionsFragment : BottomSheetDialogFragment() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setStyle(DialogFragment.STYLE_NO_TITLE, R.style.FirefoxAccountsDialogStyle)
    }

    override fun onCreateView(inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?): View? {
        return inflater.inflate(R.layout.fragment_pair_instructions, container, false)
    }

    override fun onResume() {
        super.onResume()
        (activity as AppCompatActivity).title = getString(R.string.preferences_sync)
        (activity as AppCompatActivity).supportActionBar?.show()
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        val instructionsText = view.findViewById(R.id.pair_instructions_info) as TextView
        instructionsText.text = HtmlCompat.fromHtml(
            getString(R.string.pair_instructions),
            HtmlCompat.FROM_HTML_MODE_LEGACY
        )

        val openCamera = view.findViewById(R.id.pair_open_camera) as Button
        openCamera.setOnClickListener {
            val directions = PairInstructionsFragmentDirections.actionPairInstructionsFragmentToPairFragment()
            findNavController(this@PairInstructionsFragment).navigate(directions)
        }

        val cancelCamera = view.findViewById(R.id.pair_cancel) as Button
        cancelCamera.setOnClickListener {
            findNavController(this@PairInstructionsFragment).navigateUp()
        }
    }
}
