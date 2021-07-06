/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.view.View
import androidx.recyclerview.widget.RecyclerView
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.databinding.OnboardingPrivacyNoticeBinding
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.home.sessioncontrol.OnboardingInteractor

class OnboardingPrivacyNoticeViewHolder(
    view: View,
    private val interactor: OnboardingInteractor
) : RecyclerView.ViewHolder(view) {

    init {
        val binding = OnboardingPrivacyNoticeBinding.bind(view)
        binding.headerText.setOnboardingIcon(R.drawable.ic_info)

        val appName = view.context.getString(R.string.app_name)
        binding.descriptionText.text = view.context.getString(R.string.onboarding_privacy_notice_description2, appName)

        binding.readButton.setOnClickListener {
            it.context.components.analytics.metrics.track(Event.OnboardingPrivacyNotice)
            interactor.onReadPrivacyNoticeClicked()
        }
    }

    companion object {
        const val LAYOUT_ID = R.layout.onboarding_privacy_notice
    }
}
