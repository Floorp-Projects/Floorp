/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.view.View
import androidx.navigation.Navigation
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.service.glean.private.NoExtras
import org.mozilla.fenix.GleanMetrics.Onboarding
import org.mozilla.fenix.R
import org.mozilla.fenix.components.accounts.FenixFxAEntryPoint
import org.mozilla.fenix.databinding.OnboardingManualSigninBinding
import org.mozilla.fenix.home.HomeFragmentDirections

class OnboardingManualSignInViewHolder(view: View) : RecyclerView.ViewHolder(view) {
    private var binding: OnboardingManualSigninBinding = OnboardingManualSigninBinding.bind(view)

    init {
        binding.fxaSignInButton.setOnClickListener {
            Onboarding.fxaManualSignin.record(NoExtras())

            val directions = HomeFragmentDirections.actionGlobalTurnOnSync(
                entrypoint = FenixFxAEntryPoint.OnboardingManualSignIn,
            )
            Navigation.findNavController(view).navigate(directions)
        }
    }

    companion object {
        const val LAYOUT_ID = R.layout.onboarding_manual_signin
    }
}
