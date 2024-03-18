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
import org.mozilla.fenix.GleanMetrics.CustomizeHome
import org.mozilla.fenix.R
import org.mozilla.fenix.components.appstate.AppAction
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.navigateWithBreadcrumb
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.utils.view.addToRadioGroup

/**
 * Lets the user customize the home screen.
 */
class HomeSettingsFragment : PreferenceFragmentCompat() {

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.home_preferences, rootKey)
        setupPreferences()
    }

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.preferences_home_2))
    }

    private fun setupPreferences() {
        requirePreference<SwitchPreference>(R.string.pref_key_show_top_sites).apply {
            isChecked = context.settings().showTopSitesFeature
            onPreferenceChangeListener = object : SharedPreferenceUpdater() {
                override fun onPreferenceChange(preference: Preference, newValue: Any?): Boolean {
                    CustomizeHome.preferenceToggled.record(
                        CustomizeHome.PreferenceToggledExtra(
                            newValue as Boolean,
                            "most_visited_sites",
                        ),
                    )

                    return super.onPreferenceChange(preference, newValue)
                }
            }
        }

        requirePreference<CheckBoxPreference>(R.string.pref_key_enable_contile).apply {
            isChecked = context.settings().showContileFeature
            onPreferenceChangeListener = object : SharedPreferenceUpdater() {
                override fun onPreferenceChange(preference: Preference, newValue: Any?): Boolean {
                    CustomizeHome.preferenceToggled.record(
                        CustomizeHome.PreferenceToggledExtra(
                            newValue as Boolean,
                            "contile",
                        ),
                    )

                    return super.onPreferenceChange(preference, newValue)
                }
            }
        }

        requirePreference<SwitchPreference>(R.string.pref_key_recent_tabs).apply {
            isChecked = context.settings().showRecentTabsFeature
            onPreferenceChangeListener = object : SharedPreferenceUpdater() {
                override fun onPreferenceChange(preference: Preference, newValue: Any?): Boolean {
                    CustomizeHome.preferenceToggled.record(
                        CustomizeHome.PreferenceToggledExtra(
                            newValue as Boolean,
                            "jump_back_in",
                        ),
                    )

                    return super.onPreferenceChange(preference, newValue)
                }
            }
        }

        requirePreference<SwitchPreference>(R.string.pref_key_recent_bookmarks).apply {
            isChecked = context.settings().showRecentBookmarksFeature
            onPreferenceChangeListener = object : SharedPreferenceUpdater() {
                override fun onPreferenceChange(preference: Preference, newValue: Any?): Boolean {
                    CustomizeHome.preferenceToggled.record(
                        CustomizeHome.PreferenceToggledExtra(
                            newValue as Boolean,
                            "recently_saved",
                        ),
                    )

                    return super.onPreferenceChange(preference, newValue)
                }
            }
        }

        requirePreference<SwitchPreference>(R.string.pref_key_pocket_homescreen_recommendations).apply {
            isVisible = FeatureFlags.isPocketRecommendationsFeatureEnabled(context)
            isChecked = context.settings().showPocketRecommendationsFeature
            summary = context.getString(
                R.string.customize_toggle_pocket_summary,
                context.getString(R.string.pocket_product_name),
            )
            onPreferenceChangeListener = object : SharedPreferenceUpdater() {
                override fun onPreferenceChange(preference: Preference, newValue: Any?): Boolean {
                    CustomizeHome.preferenceToggled.record(
                        CustomizeHome.PreferenceToggledExtra(
                            newValue as Boolean,
                            "pocket",
                        ),
                    )

                    return super.onPreferenceChange(preference, newValue)
                }
            }
        }

        requirePreference<CheckBoxPreference>(R.string.pref_key_pocket_sponsored_stories).apply {
            isVisible = FeatureFlags.isPocketSponsoredStoriesFeatureEnabled(context)
            isChecked = context.settings().showPocketSponsoredStories
            onPreferenceChangeListener = object : SharedPreferenceUpdater() {
                override fun onPreferenceChange(preference: Preference, newValue: Any?): Boolean {
                    when (newValue) {
                        true -> {
                            context.components.core.pocketStoriesService.startPeriodicSponsoredStoriesRefresh()
                        }
                        false -> {
                            context.components.core.pocketStoriesService.deleteProfile()
                            context.components.appStore.dispatch(
                                AppAction.PocketSponsoredStoriesChange(emptyList()),
                            )
                        }
                    }

                    return super.onPreferenceChange(preference, newValue)
                }
            }
        }

        requirePreference<SwitchPreference>(R.string.pref_key_history_metadata_feature).apply {
            isChecked = context.settings().historyMetadataUIFeature
            onPreferenceChangeListener = object : SharedPreferenceUpdater() {
                override fun onPreferenceChange(preference: Preference, newValue: Any?): Boolean {
                    CustomizeHome.preferenceToggled.record(
                        CustomizeHome.PreferenceToggledExtra(
                            newValue as Boolean,
                            "recently_visited",
                        ),
                    )

                    return super.onPreferenceChange(preference, newValue)
                }
            }
        }

        val openingScreenRadioHomepage =
            requirePreference<RadioButtonPreference>(R.string.pref_key_start_on_home_always)
        val openingScreenLastTab =
            requirePreference<RadioButtonPreference>(R.string.pref_key_start_on_home_never)
        val openingScreenAfterFourHours =
            requirePreference<RadioButtonPreference>(R.string.pref_key_start_on_home_after_four_hours)

        requirePreference<Preference>(R.string.pref_key_wallpapers).apply {
            setOnPreferenceClickListener {
                view?.findNavController()?.navigateWithBreadcrumb(
                    directions = HomeSettingsFragmentDirections.actionHomeSettingsFragmentToWallpaperSettingsFragment(),
                    navigateFrom = "HomeSettingsFragment",
                    navigateTo = "ActionHomeSettingsFragmentToWallpaperSettingsFragment",
                    crashReporter = context.components.analytics.crashReporter,
                )
                true
            }
        }

        addToRadioGroup(
            openingScreenRadioHomepage,
            openingScreenLastTab,
            openingScreenAfterFourHours,
        )
    }
}
