/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.content.Context
import android.os.Build
import android.os.Build.VERSION.SDK_INT
import android.view.View
import androidx.appcompat.app.AppCompatDelegate
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.onboarding_section_header.view.*
import kotlinx.android.synthetic.main.onboarding_theme_picker.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.utils.Settings

class OnboardingThemePickerViewHolder(private val view: View) : RecyclerView.ViewHolder(view) {

    fun bind(labelBuilder: (Context) -> String) {
        view.section_header_text.text = labelBuilder(view.context)
    }

    companion object {
        const val LAYOUT_ID = R.layout.onboarding_theme_picker
    }

    init {
        val radioLightTheme = view.theme_light_radio_button
        val radioDarkTheme = view.theme_dark_radio_button
        val radioFollowDeviceTheme = view.theme_automatic_radio_button

        if (SDK_INT >= Build.VERSION_CODES.P) {
            radioFollowDeviceTheme?.key = R.string.pref_key_follow_device_theme
        } else {
            radioFollowDeviceTheme?.key = R.string.pref_key_auto_battery_theme
        }

        radioLightTheme.addToRadioGroup(radioDarkTheme)
        radioDarkTheme.addToRadioGroup(radioLightTheme)

        radioLightTheme.addToRadioGroup(radioFollowDeviceTheme)
        radioDarkTheme.addToRadioGroup(radioFollowDeviceTheme)

        radioFollowDeviceTheme.addToRadioGroup(radioDarkTheme)
        radioFollowDeviceTheme.addToRadioGroup(radioLightTheme)

        view.theme_dark_image.setOnClickListener {
            radioDarkTheme.performClick()
        }

        view.theme_light_image.setOnClickListener {
            radioLightTheme.performClick()
        }

        val automaticTitle = view.context.getString(R.string.onboarding_theme_automatic_title)
        val automaticSummary = view.context.getString(R.string.onboarding_theme_automatic_summary)
        view.clickable_region_automatic.contentDescription = "$automaticTitle $automaticSummary"

        view.clickable_region_automatic.setOnClickListener {
            radioFollowDeviceTheme.performClick()
        }

        radioLightTheme.onClickListener {
            setNewTheme(AppCompatDelegate.MODE_NIGHT_NO)
        }

        radioDarkTheme.onClickListener {
            setNewTheme(AppCompatDelegate.MODE_NIGHT_YES)
        }

        radioFollowDeviceTheme.onClickListener {
            if (SDK_INT >= Build.VERSION_CODES.P) {
                setNewTheme(AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM)
            } else {
                setNewTheme(AppCompatDelegate.MODE_NIGHT_AUTO_BATTERY)
            }
        }

        with(Settings.getInstance(view.context)) {
            when {
                this.shouldUseLightTheme -> radioLightTheme.isChecked = true
                this.shouldUseDarkTheme -> radioDarkTheme.isChecked = true
                else -> radioFollowDeviceTheme.isChecked = true
            }
        }
    }

    private fun setNewTheme(mode: Int) {
        if (AppCompatDelegate.getDefaultNightMode() == mode) return
        AppCompatDelegate.setDefaultNightMode(mode)
        view.context?.components?.core?.let {
            it.engine.settings.preferredColorScheme = it.getPreferredColorScheme()
        }
        view.context?.components?.useCases?.sessionUseCases?.reload?.invoke()
    }
}
