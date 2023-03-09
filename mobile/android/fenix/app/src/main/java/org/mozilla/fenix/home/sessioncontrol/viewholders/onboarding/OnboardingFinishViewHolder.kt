/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.view.View
import androidx.recyclerview.widget.RecyclerView
import mozilla.components.service.glean.private.NoExtras
import org.mozilla.fenix.GleanMetrics.Onboarding
import org.mozilla.fenix.R
import org.mozilla.fenix.databinding.OnboardingFinishBinding
import org.mozilla.fenix.onboarding.interactor.OnboardingInteractor

class OnboardingFinishViewHolder(
    view: View,
    private val interactor: OnboardingInteractor,
) : RecyclerView.ViewHolder(view) {

    init {
        val binding = OnboardingFinishBinding.bind(view)
        binding.finishButton.setOnClickListener {
            interactor.onStartBrowsingClicked()
            Onboarding.finish.record(NoExtras())
        }
    }

    companion object {
        const val LAYOUT_ID = R.layout.onboarding_finish
    }
}
