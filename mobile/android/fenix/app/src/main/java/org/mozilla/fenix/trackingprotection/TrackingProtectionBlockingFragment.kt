/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.trackingprotection

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.Fragment
import kotlinx.android.synthetic.main.fragment_tracking_protection_blocking.*
import org.mozilla.fenix.R

class TrackingProtectionBlockingFragment : Fragment() {
    private val safeArguments get() = requireNotNull(arguments)

    private val isStrict: Boolean by lazy {
        TrackingProtectionBlockingFragmentArgs.fromBundle(safeArguments).strictMode
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        return inflater.inflate(R.layout.fragment_tracking_protection_blocking, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        if (isStrict) {
            category_tracking_content.visibility = View.VISIBLE
        } else {
            category_tracking_content.visibility = View.GONE
        }
    }

    override fun onResume() {
        super.onResume()
        (activity as AppCompatActivity).title =
            getString(
                if (isStrict) R.string.preference_enhanced_tracking_protection_strict else
                    R.string.preference_enhanced_tracking_protection_standard
            )
        (activity as AppCompatActivity).supportActionBar?.show()
    }
}
