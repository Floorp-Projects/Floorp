/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.view.View
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.onboarding_tracking_protection.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.utils.Settings

class OnboardingTrackingProtectionViewHolder(view: View) : RecyclerView.ViewHolder(view) {

    init {
        view.header_text.setOnboardingIcon(R.drawable.ic_onboarding_tracking_protection)

        val appName = view.context.getString(R.string.app_name)
        view.description_text.text = view.context.getString(
            R.string.onboarding_tracking_protection_description,
            appName
        )

        view.tracking_protection_toggle.apply {
            isChecked = Settings.getInstance(view.context).shouldUseTrackingProtection
            setOnCheckedChangeListener { _, isChecked ->
                updateTrackingProtectionSetting(isChecked)
            }
        }
    }

    private fun updateTrackingProtectionSetting(enabled: Boolean) {
        Settings.getInstance(itemView.context).shouldUseTrackingProtection = enabled
        with(itemView.context.components) {
            val policy = core.createTrackingProtectionPolicy(enabled)
            useCases.settingsUseCases.updateTrackingProtection.invoke(policy)
            useCases.sessionUseCases.reload.invoke()
        }
    }

    companion object {
        const val LAYOUT_ID = R.layout.onboarding_tracking_protection
    }
}
