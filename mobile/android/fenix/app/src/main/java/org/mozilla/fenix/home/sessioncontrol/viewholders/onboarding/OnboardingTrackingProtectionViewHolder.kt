/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.view.View
import androidx.appcompat.content.res.AppCompatResources
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.onboarding_tracking_protection.view.*
import org.jetbrains.anko.dimen
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.utils.Settings

class OnboardingTrackingProtectionViewHolder(val view: View) : RecyclerView.ViewHolder(view) {

    init {
        val icon = AppCompatResources.getDrawable(view.context, R.drawable.ic_onboarding_tracking_protection)
        val size = view.context.dimen(R.dimen.onboarding_header_icon_height_width)
        icon?.setBounds(0, 0, size, size)

        view.header_text.setCompoundDrawables(icon, null, null, null)

        val appName = view.context.getString(R.string.app_name)
        view.description_text.text = view.context.getString(
            R.string.onboarding_tracking_protection_description,
            appName
        )

        val switch = view.tracking_protection_toggle

        switch.isChecked = Settings.getInstance(view.context).shouldUseTrackingProtection

        switch.setOnCheckedChangeListener { _, isChecked ->
            updateTrackingProtectionSetting(isChecked)
        }
    }

    private fun updateTrackingProtectionSetting(enabled: Boolean) {
        Settings.getInstance(view.context).setTrackingProtection(enabled)
        with(view.context.components) {
            val policy = core.createTrackingProtectionPolicy(enabled)
            useCases.settingsUseCases.updateTrackingProtection.invoke(policy)
            useCases.sessionUseCases.reload.invoke()
        }

        view.context.components.useCases.sessionUseCases.reload.invoke()
    }

    companion object {
        const val LAYOUT_ID = R.layout.onboarding_tracking_protection
    }
}
