/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.sessioncontrol.viewholders.onboarding

import android.os.Build
import android.os.Build.VERSION.SDK_INT
import android.view.View
import androidx.appcompat.app.AppCompatDelegate
import androidx.recyclerview.widget.RecyclerView
import kotlinx.android.synthetic.main.onboarding_theme_picker.view.*
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.Event.OnboardingThemePicker.Theme
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.onboarding.OnboardingRadioButton
import org.mozilla.fenix.utils.view.addToRadioGroup

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

        addToRadioGroup(
            radioLightTheme,
            radioDarkTheme,
            radioFollowDeviceTheme
        )
        radioLightTheme.addIllustration(view.theme_light_image)
        radioDarkTheme.addIllustration(view.theme_dark_image)

        view.theme_dark_image.setOnClickListener {
            it.context.components.analytics.metrics.track(Event.OnboardingThemePicker(Theme.DARK))
            radioDarkTheme.performClick()
        }

        view.theme_light_image.setOnClickListener {
            it.context.components.analytics.metrics.track(Event.OnboardingThemePicker(Theme.LIGHT))
            radioLightTheme.performClick()
        }

        val automaticTitle = view.context.getString(R.string.onboarding_theme_automatic_title)
        val automaticSummary = view.context.getString(R.string.onboarding_theme_automatic_summary)
        view.clickable_region_automatic.contentDescription = "$automaticTitle $automaticSummary"

        view.clickable_region_automatic.setOnClickListener {
            it.context.components.analytics.metrics
                .track(Event.OnboardingThemePicker(Theme.FOLLOW_DEVICE))
            radioFollowDeviceTheme.performClick()
        }

        radioLightTheme.onClickListener {
            view.context.components.analytics.metrics
                .track(Event.OnboardingThemePicker(Theme.LIGHT))
            setNewTheme(AppCompatDelegate.MODE_NIGHT_NO)
        }

        radioDarkTheme.onClickListener {
            view.context.components.analytics.metrics
                .track(Event.OnboardingThemePicker(Theme.DARK))
            setNewTheme(AppCompatDelegate.MODE_NIGHT_YES)
        }

        radioFollowDeviceTheme.onClickListener {
            view.context.components.analytics.metrics
                .track(Event.OnboardingThemePicker(Theme.FOLLOW_DEVICE))
            if (SDK_INT >= Build.VERSION_CODES.P) {
                setNewTheme(AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM)
            } else {
                setNewTheme(AppCompatDelegate.MODE_NIGHT_AUTO_BATTERY)
            }
        }

        with(view.context.settings()) {
            val radio: OnboardingRadioButton = when {
                shouldUseLightTheme -> {
                    radioLightTheme
                }
                shouldUseDarkTheme -> {
                    radioDarkTheme
                }
                else -> {
                    radioFollowDeviceTheme
                }
            }
            radio.updateRadioValue(true)
        }
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
