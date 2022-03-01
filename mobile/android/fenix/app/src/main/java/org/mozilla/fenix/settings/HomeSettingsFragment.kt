/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.os.Bundle
import androidx.navigation.findNavController
import androidx.preference.CheckBoxPreference
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.SwitchPreference
import org.mozilla.fenix.FeatureFlags
import org.mozilla.fenix.R
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.utils.view.addToRadioGroup

/**
 * Lets the user customize the home screen.
 */
class HomeSettingsFragment : PreferenceFragmentCompat() {

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.home_preferences, rootKey)
    }

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.preferences_home_2))
        setupPreferences()
    }

    private fun setupPreferences() {
        requirePreference<SwitchPreference>(R.string.pref_key_enable_top_frecent_sites).apply {
            isChecked = context.settings().showTopFrecentSites
            onPreferenceChangeListener = CustomizeHomeMetricsUpdater()
        }

        requirePreference<CheckBoxPreference>(R.string.pref_key_enable_contile).apply {
            isVisible = FeatureFlags.contileFeature
            isChecked = context.settings().showContileFeature
            onPreferenceChangeListener = CustomizeHomeMetricsUpdater()
        }

        requirePreference<SwitchPreference>(R.string.pref_key_recent_tabs).apply {
            isVisible = FeatureFlags.showRecentTabsFeature
            isChecked = context.settings().showRecentTabsFeature
            onPreferenceChangeListener = CustomizeHomeMetricsUpdater()
        }

        requirePreference<SwitchPreference>(R.string.pref_key_recent_bookmarks).apply {
            isVisible = FeatureFlags.recentBookmarksFeature
            isChecked = context.settings().showRecentBookmarksFeature
            onPreferenceChangeListener = CustomizeHomeMetricsUpdater()
        }

        requirePreference<SwitchPreference>(R.string.pref_key_pocket_homescreen_recommendations).apply {
            isVisible = FeatureFlags.isPocketRecommendationsFeatureEnabled(context)
            isChecked = context.settings().showPocketRecommendationsFeature
            onPreferenceChangeListener = CustomizeHomeMetricsUpdater()
        }

        requirePreference<SwitchPreference>(R.string.pref_key_history_metadata_feature).apply {
            isVisible = FeatureFlags.historyMetadataUIFeature
            isChecked = context.settings().historyMetadataUIFeature
            onPreferenceChangeListener = CustomizeHomeMetricsUpdater()
        }

        val openingScreenRadioHomepage =
            requirePreference<RadioButtonPreference>(R.string.pref_key_start_on_home_always)
        val openingScreenLastTab =
            requirePreference<RadioButtonPreference>(R.string.pref_key_start_on_home_never)
        val openingScreenAfterFourHours =
            requirePreference<RadioButtonPreference>(R.string.pref_key_start_on_home_after_four_hours)

        requirePreference<Preference>(R.string.pref_key_wallpapers).apply {
            setOnPreferenceClickListener {
                view?.findNavController()?.navigate(
                    HomeSettingsFragmentDirections.actionHomeSettingsFragmentToWallpaperSettingsFragment()
                )
                true
            }
            isVisible = FeatureFlags.showWallpapers
        }

        addToRadioGroup(
            openingScreenRadioHomepage,
            openingScreenLastTab,
            openingScreenAfterFourHours
        )
    }

    inner class CustomizeHomeMetricsUpdater : SharedPreferenceUpdater() {
        override fun onPreferenceChange(preference: Preference, newValue: Any?): Boolean {
            try {
                val context = preference.context
                context.components.analytics.metrics.track(
                    Event.CustomizeHomePreferenceToggled(
                        preference.key,
                        newValue as Boolean,
                        context
                    )
                )
            } catch (e: IllegalArgumentException) {
                // The event is not tracked
            }
            return super.onPreferenceChange(preference, newValue)
        }
    }
}
