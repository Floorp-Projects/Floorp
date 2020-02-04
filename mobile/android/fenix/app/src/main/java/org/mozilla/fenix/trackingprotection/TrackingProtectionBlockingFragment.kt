/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.trackingprotection

import android.os.Bundle
import android.view.View
import androidx.core.view.isVisible
import androidx.fragment.app.Fragment
import androidx.navigation.fragment.navArgs
import kotlinx.android.synthetic.main.fragment_tracking_protection_blocking.*
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.showToolbar

class TrackingProtectionBlockingFragment :
    Fragment(R.layout.fragment_tracking_protection_blocking) {

    private val args: TrackingProtectionBlockingFragmentArgs by navArgs()
    private var isCustomProtection: Boolean = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        isCustomProtection = requireContext().settings().useCustomTrackingProtection
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        when (args.protectionMode) {
            getString(R.string.preference_enhanced_tracking_protection_strict) -> {
                category_fingerprinters.isVisible = true
                category_tracking_content.isVisible = true
            }

            getString(R.string.preference_enhanced_tracking_protection_custom) -> {
                category_fingerprinters.isVisible =
                    requireContext().settings().blockFingerprintersInCustomTrackingProtection
                category_cryptominers.isVisible =
                    requireContext().settings().blockCryptominersInCustomTrackingProtection
                category_cookies.isVisible =
                    requireContext().settings().blockCookiesInCustomTrackingProtection
            }

            getString(R.string.preference_enhanced_tracking_protection_standard) -> return

            else -> return
        }
    }

    override fun onResume() {
        super.onResume()
        showToolbar(args.protectionMode)
    }
}
