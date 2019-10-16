/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.content.res.AppCompatResources
import androidx.core.view.isVisible
import kotlinx.android.extensions.LayoutContainer
import kotlinx.android.synthetic.main.quicksettings_tracking_protection.*
import mozilla.components.support.ktx.android.view.putCompoundDrawablesRelativeWithIntrinsicBounds
import org.mozilla.fenix.R

interface TrackingProtectionInteractor {
    fun onReportProblemSelected(websiteUrl: String)
    fun onProtectionToggled(websiteUrl: String, trackingEnabled: Boolean)
    fun onProtectionSettingsSelected()
    fun onTrackingProtectionShown()
}

class TrackingProtectionView(
    override val containerView: ViewGroup,
    val interactor: TrackingProtectionInteractor
) : LayoutContainer {

    val view: View = LayoutInflater.from(containerView.context)
        .inflate(R.layout.quicksettings_tracking_protection, containerView, true)

    init {
        trackingProtectionSwitch.putCompoundDrawablesRelativeWithIntrinsicBounds(
            start = AppCompatResources.getDrawable(
                containerView.context,
                R.drawable.ic_tracking_protection
            )
        )
    }

    fun update(state: TrackingProtectionState) {
        if (state.isVisible) {
            interactor.onTrackingProtectionShown()
        }

        reportSiteIssueAction.setOnClickListener { interactor.onReportProblemSelected(state.websiteUrl) }

        trackingProtectionAction.isVisible = !state.isTrackingProtectionEnabledPerApp
        if (!state.isTrackingProtectionEnabledPerApp) {
            trackingProtectionAction.setOnClickListener { interactor.onProtectionSettingsSelected() }
        }

        trackingProtectionSwitch.isChecked = state.isTrackingProtectionEnabledPerWebsite
        trackingProtectionSwitch.isEnabled = state.isTrackingProtectionEnabledPerApp
        if (state.isTrackingProtectionEnabledPerApp) {
            trackingProtectionSwitch.setOnCheckedChangeListener { _, isChecked ->
                interactor.onProtectionToggled(state.websiteUrl, isChecked)
            }
        }
    }
}
