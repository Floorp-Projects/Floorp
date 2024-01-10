/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings

import android.content.SharedPreferences
import android.os.Bundle
import androidx.preference.Preference
import mozilla.components.service.glean.private.NoExtras
import org.mozilla.focus.GleanMetrics.SearchEngines
import org.mozilla.focus.GleanMetrics.ShowSearchSuggestions
import org.mozilla.focus.R
import org.mozilla.focus.ext.requireComponents
import org.mozilla.focus.ext.showToolbar
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.state.Screen

class SearchSettingsFragment :
    BaseSettingsFragment(),
    SharedPreferences.OnSharedPreferenceChangeListener {
    override fun onCreatePreferences(p0: Bundle?, p1: String?) {
        addPreferencesFromResource(R.xml.search_settings)
    }

    override fun onResume() {
        super.onResume()

        preferenceManager.sharedPreferences?.registerOnSharedPreferenceChangeListener(this)

        showToolbar(getString(R.string.preference_category_search))
    }

    override fun onPause() {
        preferenceManager.sharedPreferences?.unregisterOnSharedPreferenceChangeListener(this)
        super.onPause()
    }

    override fun onPreferenceTreeClick(preference: Preference): Boolean {
        when (preference.key) {
            resources.getString(R.string.pref_key_search_engine) -> run {
                requireComponents.appStore.dispatch(
                    AppAction.OpenSettings(page = Screen.Settings.Page.SearchList),
                )
                SearchEngines.openSettings.record(NoExtras())
            }
            resources.getString(R.string.pref_key_screen_autocomplete) ->
                requireComponents.appStore.dispatch(
                    AppAction.OpenSettings(page = Screen.Settings.Page.SearchAutocomplete),
                )
        }
        return super.onPreferenceTreeClick(preference)
    }

    override fun onSharedPreferenceChanged(sharedPreferences: SharedPreferences, key: String?) {
        when (key) {
            resources.getString(R.string.pref_key_show_search_suggestions) ->
                ShowSearchSuggestions.changedFromSettings.record(
                    ShowSearchSuggestions.ChangedFromSettingsExtra(sharedPreferences.getBoolean(key, false)),
                )
        }
    }

    companion object {

        fun newInstance(): SearchSettingsFragment {
            return SearchSettingsFragment()
        }
    }
}
