/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.view.View
import androidx.appcompat.widget.SwitchCompat
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.onboarding_tracking_protection.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.Event.OnboardingTrackingProtection.Setting
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.onboarding.OnboardingRadioButton
import org.mozilla.fenix.utils.view.addToRadioGroup

class OnboardingTrackingProtectionViewHolder(view: View) : RecyclerView.ViewHolder(view) {

    private var standardTrackingProtection: OnboardingRadioButton
    private var strictTrackingProtection: OnboardingRadioButton
    private var trackingProtectionToggle: SwitchCompat

    init {
        view.header_text.setOnboardingIcon(R.drawable.ic_onboarding_tracking_protection)

        trackingProtectionToggle = view.tracking_protection_toggle
        standardTrackingProtection = view.tracking_protection_standard_option
        strictTrackingProtection = view.tracking_protection_strict_default

        view.description_text.text = view.context.getString(
            R.string.onboarding_tracking_protection_description_2
        )

        trackingProtectionToggle.apply {
            isChecked = view.context.settings().shouldUseTrackingProtection
            setOnCheckedChangeListener { _, isChecked ->
                updateTrackingProtectionSetting(isChecked)
                updateRadioGroupState(isChecked)
            }
        }

        setupRadioGroup(trackingProtectionToggle.isChecked)
        updateRadioGroupState(trackingProtectionToggle.isChecked)
    }

    private fun setupRadioGroup(isChecked: Boolean) {

        updateRadioGroupState(isChecked)

        addToRadioGroup(standardTrackingProtection, strictTrackingProtection)

        strictTrackingProtection.isChecked =
            itemView.context.settings().useStrictTrackingProtection
        standardTrackingProtection.isChecked =
            !itemView.context.settings().useStrictTrackingProtection

        standardTrackingProtection.onClickListener {
            updateTrackingProtectionPolicy()
            itemView.context.components.analytics.metrics
                .track(Event.OnboardingTrackingProtection(Setting.STANDARD))
        }

        strictTrackingProtection.onClickListener {
            updateTrackingProtectionPolicy()
            itemView.context.components.analytics.metrics
                .track(Event.OnboardingTrackingProtection(Setting.STRICT))
        }
    }

    private fun updateRadioGroupState(isChecked: Boolean) {
        standardTrackingProtection.isEnabled = isChecked
        strictTrackingProtection.isEnabled = isChecked
    }

    private fun updateTrackingProtectionSetting(enabled: Boolean) {
        itemView.context.settings().shouldUseTrackingProtection = enabled
        with(itemView.context.components) {
            val policy = core.trackingProtectionPolicyFactory.createTrackingProtectionPolicy()
            useCases.settingsUseCases.updateTrackingProtection.invoke(policy)
            useCases.sessionUseCases.reload.invoke()
        }
    }

    private fun updateTrackingProtectionPolicy() {
        itemView.context?.components?.let {
            val policy = it.core.trackingProtectionPolicyFactory
                .createTrackingProtectionPolicy()
            it.useCases.settingsUseCases.updateTrackingProtection.invoke(policy)
            it.useCases.sessionUseCases.reload.invoke()
        }
    }

    companion object {
        const val LAYOUT_ID = R.layout.onboarding_tracking_protection
    }
}
