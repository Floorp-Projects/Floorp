/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings

import android.content.SharedPreferences
import android.os.Build
import android.os.Bundle
import androidx.appcompat.app.AppCompatDelegate
import androidx.preference.Preference
import androidx.preference.PreferenceManager
import org.mozilla.focus.R
import org.mozilla.focus.ext.requireComponents
import org.mozilla.focus.ext.requirePreference
import org.mozilla.focus.ext.showToolbar
import org.mozilla.focus.locale.screen.LanguageStorage.Companion.LOCALE_SYSTEM_DEFAULT
import org.mozilla.focus.locale.screen.LocaleDescriptor
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.state.Screen
import org.mozilla.focus.widget.DefaultBrowserPreference

@Suppress("TooManyFunctions") // code is split into multiple functions with their own purpose.
class GeneralSettingsFragment :
    BaseSettingsFragment() {

    private lateinit var radioLightTheme: RadioButtonPreference
    private lateinit var radioDarkTheme: RadioButtonPreference
    private lateinit var radioDefaultTheme: RadioButtonPreference

    private lateinit var defaultBrowserPreference: DefaultBrowserPreference

    override fun onCreatePreferences(p0: Bundle?, p1: String?) {
        addPreferencesFromResource(R.xml.general_settings)
        setupPreferences()
    }

    override fun onResume() {
        super.onResume()
        defaultBrowserPreference.update()
        showToolbar(getString(R.string.preference_category_general))
    }

    private fun setupPreferences() {
        setupDefaultBrowserPreference()
        bindLocalePreference()
        bindLightTheme()
        bindDarkTheme()
        bindDefaultTheme()
        setupRadioGroups()
    }

    private fun bindLocalePreference() {
        val localePreference: Preference = requirePreference(R.string.pref_key_locale)
        localePreference.summary = getLocaleSummary()
        localePreference.setOnPreferenceClickListener {
            requireComponents.appStore.dispatch(
                AppAction.OpenSettings(Screen.Settings.Page.Locale),
            )
            true
        }
    }

    private fun setupDefaultBrowserPreference() {
        defaultBrowserPreference = requirePreference(R.string.pref_key_default_browser)
    }

    private fun bindLightTheme() {
        radioLightTheme = requirePreference(R.string.pref_key_light_theme)
        radioLightTheme.onClickListener {
            setNewTheme(AppCompatDelegate.MODE_NIGHT_NO)
        }
    }

    private fun bindDarkTheme() {
        radioDarkTheme = requirePreference(R.string.pref_key_dark_theme)
        radioDarkTheme.onClickListener {
            setNewTheme(AppCompatDelegate.MODE_NIGHT_YES)
        }
    }

    private fun bindDefaultTheme() {
        radioDefaultTheme = requirePreference(R.string.pref_key_default_theme)
        val defaultThemeTitle = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            context?.getString(R.string.preference_follow_device_theme)
        } else {
            context?.getString(R.string.preference_auto_battery_theme)
        }

        radioDefaultTheme.apply {
            title = defaultThemeTitle
            onClickListener {
                setDefaultTheme()
            }
        }
    }

    private fun getLocaleSummary(): CharSequence? {
        val sharedConfig: SharedPreferences = PreferenceManager.getDefaultSharedPreferences(requireContext())
        val value: String? =
            sharedConfig.getString(
                resources.getString(R.string.pref_key_locale),
                resources.getString(R.string.preference_language_systemdefault),
            )
        return value?.let {
            if (value.isEmpty() || value == LOCALE_SYSTEM_DEFAULT) {
                return resources.getString(R.string.preference_language_systemdefault)
            }
            LocaleDescriptor(it).getNativeName()
        }
    }

    private fun setupRadioGroups() {
        addToRadioGroup(
            radioLightTheme,
            radioDarkTheme,
            radioDefaultTheme,
        )
    }

    private fun setNewTheme(mode: Int) {
        if (AppCompatDelegate.getDefaultNightMode() == mode) return
        AppCompatDelegate.setDefaultNightMode(mode)
        activity?.recreate()

        requireComponents.engine.settings.preferredColorScheme = requireComponents.settings.getPreferredColorScheme()
        requireComponents.sessionUseCases.reload.invoke()
    }

    private fun setDefaultTheme() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            AppCompatDelegate.setDefaultNightMode(
                AppCompatDelegate.MODE_NIGHT_FOLLOW_SYSTEM,
            )
        } else {
            AppCompatDelegate.setDefaultNightMode(
                AppCompatDelegate.MODE_NIGHT_AUTO_BATTERY,
            )
        }
    }
}
