/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.os.Build
import android.os.Build.VERSION.SDK_INT
import android.view.View
import androidx.appcompat.app.AppCompatDelegate
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.onboarding_theme_picker.view.clickable_region_automatic
import kotlinx.android.synthetic.main.onboarding_theme_picker.view.theme_automatic_radio_button
import kotlinx.android.synthetic.main.onboarding_theme_picker.view.theme_dark_image
import kotlinx.android.synthetic.main.onboarding_theme_picker.view.theme_dark_radio_button
import kotlinx.android.synthetic.main.onboarding_theme_picker.view.theme_light_image
import kotlinx.android.synthetic.main.onboarding_theme_picker.view.theme_light_radio_button
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.onboarding.OnboardingRadioButton

class OnboardingThemePickerViewHolder(view: View) : RecyclerView.ViewHolder(view) {

    init {
        val radioLightTheme = view.theme_light_radio_button
        val radioDarkTheme = view.theme_dark_radio_button
        val radioFollowDeviceTheme = view.theme_automatic_radio_button

        radioFollowDeviceTheme.key = if (SDK_INT >= Build.VERSION_CODES.P) {
            R.string.pref_key_follow_device_theme
        } else {
            R.string.pref_key_auto_battery_theme
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
            setLightIllustrationSelected()
            setNewTheme(AppCompatDelegate.MODE_NIGHT_NO)
        }

        radioDarkTheme.onClickListener {
            setDarkIllustrationSelected()
            view.context.components.analytics.metrics.track(
                Event.DarkThemeSelected(
                    Event.DarkThemeSelected.Source.ONBOARDING
                )
            )
            setNewTheme(AppCompatDelegate.MODE_NIGHT_YES)
        }

        radioFollowDeviceTheme.onClickListener {
            setNoIllustrationSelected()
            if (SDK_INT >= Build.VERSION_CODES.P) {
                setNewTheme(AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM)
            } else {
                setNewTheme(AppCompatDelegate.MODE_NIGHT_AUTO_BATTERY)
            }
        }

        with(view.context.settings()) {
            val radio: OnboardingRadioButton
            when {
                shouldUseLightTheme -> {
                    radio = radioLightTheme
                    setLightIllustrationSelected()
                }
                shouldUseDarkTheme -> {
                    radio = radioDarkTheme
                    setDarkIllustrationSelected()
                }
                else -> {
                    radio = radioFollowDeviceTheme
                    setNoIllustrationSelected()
                }
            }
            radio.isChecked = true
        }
    }

    private fun setNoIllustrationSelected() {
        itemView.theme_dark_image.isSelected = false
        itemView.theme_light_image.isSelected = false
    }

    private fun setDarkIllustrationSelected() {
        itemView.theme_dark_image.isSelected = true
        itemView.theme_light_image.isSelected = false
    }

    private fun setLightIllustrationSelected() {
        itemView.theme_dark_image.isSelected = false
        itemView.theme_light_image.isSelected = true
    }

    private fun setNewTheme(mode: Int) {
        if (AppCompatDelegate.getDefaultNightMode() == mode) return
        AppCompatDelegate.setDefaultNightMode(mode)
        with(itemView.context.components) {
            core.engine.settings.preferredColorScheme = core.getPreferredColorScheme()
            useCases.sessionUseCases.reload.invoke()
        }
    }

    companion object {
        const val LAYOUT_ID = R.layout.onboarding_theme_picker
    }
}
