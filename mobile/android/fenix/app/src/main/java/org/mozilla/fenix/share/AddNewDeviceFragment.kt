/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.share

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.Fragment
import kotlinx.android.synthetic.main.fragment_add_new_device.*
import org.mozilla.fenix.R
import org.mozilla.fenix.settings.SupportUtils

class AddNewDeviceFragment : Fragment() {
    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? = inflater.inflate(R.layout.fragment_add_new_device, container, false)

    override fun onResume() {
        super.onResume()
        (activity as AppCompatActivity).title = getString(R.string.sync_add_new_device_title)
        (activity as AppCompatActivity).supportActionBar?.show()
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        learn_button.setOnClickListener {
            val intent = SupportUtils.createCustomTabIntent(
                requireContext(),
                SupportUtils.getSumoURLForTopic(requireContext(), SupportUtils.SumoTopic.SEND_TABS)
            )
            startActivity(intent)
        }

        connect_button.setOnClickListener {
            AlertDialog.Builder(requireContext()).apply {
                setMessage(R.string.sync_connect_device_dialog)
                setPositiveButton(R.string.sync_confirmation_button) { dialog, _ -> dialog.cancel() }
                create()
            }.show()
        }
    }
}
