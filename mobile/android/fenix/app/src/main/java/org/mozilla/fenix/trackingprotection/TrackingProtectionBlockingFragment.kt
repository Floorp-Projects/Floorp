/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.trackingprotection

import android.os.Bundle
import android.view.View
import androidx.core.view.isVisible
import androidx.fragment.app.Fragment
import androidx.navigation.fragment.navArgs
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.FragmentTrackingProtectionBlockingBinding
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.showToolbar

class TrackingProtectionBlockingFragment :
    Fragment(R.layout.fragment_tracking_protection_blocking) {

    private val args: TrackingProtectionBlockingFragmentArgs by navArgs()
    private val settings by lazy { requireContext().settings() }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        val binding = FragmentTrackingProtectionBlockingBinding.bind(view)
        when (args.protectionMode) {
            TrackingProtectionMode.STANDARD -> {
                binding.categoryTrackingContent.isVisible = false
            }

            TrackingProtectionMode.STRICT -> {}

            TrackingProtectionMode.CUSTOM -> {
                binding.categoryFingerprinters.isVisible =
                    settings.blockFingerprintersInCustomTrackingProtection
                binding.categoryCryptominers.isVisible =
                    settings.blockCryptominersInCustomTrackingProtection
                binding.categoryCookies.isVisible =
                    settings.blockCookiesInCustomTrackingProtection
                binding.categoryTrackingContent.isVisible =
                    settings.blockTrackingContentInCustomTrackingProtection
                binding.categoryRedirectTrackers.isVisible =
                    settings.blockRedirectTrackersInCustomTrackingProtection
            }
        }
    }

    override fun onResume() {
        super.onResume()
        showToolbar(getString(args.protectionMode.titleRes))
    }
}
