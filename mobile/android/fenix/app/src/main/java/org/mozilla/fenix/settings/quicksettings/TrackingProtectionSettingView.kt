/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.quicksettings

import android.view.View
import android.widget.CompoundButton
import android.widget.Switch
import android.widget.TextView
import androidx.appcompat.content.res.AppCompatResources
import androidx.core.view.isVisible
import io.reactivex.Observer
import mozilla.components.support.ktx.android.view.putCompoundDrawablesRelativeWithIntrinsicBounds
import org.mozilla.fenix.R
import org.mozilla.fenix.utils.Settings

class TrackingProtectionSettingView(
    container: View,
    private val actionEmitter: Observer<QuickSettingsAction>
) : View.OnClickListener, CompoundButton.OnCheckedChangeListener {
    private val trackingProtectionSwitch: Switch = container.findViewById(R.id.tracking_protection)
    private val trackingProtectionAction: TextView = container.findViewById(R.id.tracking_protection_action)

    init {
        trackingProtectionSwitch.putCompoundDrawablesRelativeWithIntrinsicBounds(
            start = AppCompatResources.getDrawable(container.context, R.drawable.ic_tracking_protection)
        )
    }

    fun bind(isTrackingProtectionOn: Boolean) {
        val globalTPSetting = Settings.getInstance(trackingProtectionSwitch.context).shouldUseTrackingProtection

        trackingProtectionAction.isVisible = !globalTPSetting
        trackingProtectionAction.setOnClickListener(this)

        trackingProtectionSwitch.isChecked = isTrackingProtectionOn
        trackingProtectionSwitch.isEnabled = globalTPSetting
        trackingProtectionSwitch.setOnCheckedChangeListener(this)
    }

    override fun onClick(view: View) {
        actionEmitter.onNext(
            QuickSettingsAction.SelectTrackingProtectionSettings
        )
    }

    override fun onCheckedChanged(buttonView: CompoundButton, isChecked: Boolean) {
        actionEmitter.onNext(
            QuickSettingsAction.ToggleTrackingProtection(isChecked)
        )
    }
}
