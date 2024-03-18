/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.autocomplete

import android.content.SharedPreferences
import android.os.Bundle
import androidx.preference.Preference
import org.mozilla.focus.GleanMetrics.Autocomplete
import org.mozilla.focus.R
import org.mozilla.focus.ext.requireComponents
import org.mozilla.focus.ext.requirePreference
import org.mozilla.focus.ext.showToolbar
import org.mozilla.focus.settings.BaseSettingsFragment
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.state.Screen

/**
 * Settings UI for configuring autocomplete.
 */
class AutocompleteSettingsFragment : BaseSettingsFragment(), SharedPreferences.OnSharedPreferenceChangeListener {

    private lateinit var topSitesAutocomplete: AutocompleteDefaultDomainsPreference
    private lateinit var favoriteSitesAutocomplete: AutocompleteCustomDomainsPreference

    override fun onCreatePreferences(p0: Bundle?, p1: String?) {
        addPreferencesFromResource(R.xml.autocomplete)
        val appName = requireContext().getString(R.string.app_name)

        topSitesAutocomplete =
            requirePreference<AutocompleteDefaultDomainsPreference>(R.string.pref_key_autocomplete_preinstalled).apply {
                summary =
                    context.getString(R.string.preference_autocomplete_topsite_summary2, appName)
            }
        favoriteSitesAutocomplete =
            requirePreference<AutocompleteCustomDomainsPreference>(R.string.pref_key_autocomplete_custom).apply {
                summary =
                    context.getString(R.string.preference_autocomplete_user_list_summary2, appName)
            }
    }

    override fun onResume() {
        super.onResume()

        showToolbar(getString(R.string.preference_subitem_autocomplete))

        preferenceManager.sharedPreferences?.registerOnSharedPreferenceChangeListener(this)
    }

    override fun onPause() {
        super.onPause()

        preferenceManager.sharedPreferences?.unregisterOnSharedPreferenceChangeListener(this)
    }

    override fun onPreferenceTreeClick(preference: Preference): Boolean {
        if (preference.key == getString(R.string.pref_key_screen_custom_domains)) {
            requireComponents.appStore.dispatch(
                AppAction.OpenSettings(page = Screen.Settings.Page.SearchAutocompleteList),
            )
        }

        return super.onPreferenceTreeClick(preference)
    }

    override fun onSharedPreferenceChanged(sharedPreferences: SharedPreferences?, key: String?) {
        if (key == null || sharedPreferences == null) {
            return
        }

        when (key) {
            topSitesAutocomplete.key ->
                Autocomplete.topSitesSettingChanged.record(
                    Autocomplete.TopSitesSettingChangedExtra(sharedPreferences.all[key] as Boolean),
                )

            favoriteSitesAutocomplete.key ->
                Autocomplete.favoriteSitesSettingChanged.record(
                    Autocomplete.FavoriteSitesSettingChangedExtra(sharedPreferences.all[key] as Boolean),
                )
        }
    }
}
