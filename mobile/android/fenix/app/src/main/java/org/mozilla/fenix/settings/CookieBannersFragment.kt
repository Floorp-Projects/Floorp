/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.os.Bundle
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.SwitchPreferenceCompat
import mozilla.components.concept.engine.Settings
import org.mozilla.fenix.GleanMetrics.CookieBanners
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.showToolbar

/**
 * Lets the user set up the cookie banners handling preferences.
 */
class CookieBannersFragment : PreferenceFragmentCompat() {

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.cookie_banner_preferences, rootKey)
        setupPreferences()
    }

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.preferences_cookie_banner_reduction))
    }

    private fun getEngineSettings(): Settings {
        return requireContext().components.core.engine.settings
    }

    private fun setupPreferences() {
        requirePreference<SwitchPreferenceCompat>(R.string.pref_key_cookie_banner_v1).apply {
            summary =
                getString(R.string.reduce_cookie_banner_summary_1, getString(R.string.app_name))
            onPreferenceChangeListener = object : SharedPreferenceUpdater() {
                override fun onPreferenceChange(
                    preference: Preference,
                    newValue: Any?,
                ): Boolean {
                    val metricTag = if (newValue == true) {
                        "reject_all"
                    } else {
                        "disabled"
                    }
                    requireContext().settings().shouldUseCookieBanner = newValue as Boolean
                    val mode = requireContext().settings().getCookieBannerHandling()
                    getEngineSettings().cookieBannerHandlingModePrivateBrowsing = mode
                    getEngineSettings().cookieBannerHandlingMode = mode
                    CookieBanners.settingChanged.record(CookieBanners.SettingChangedExtra(metricTag))
                    requireContext().components.useCases.sessionUseCases.reload()
                    return super.onPreferenceChange(preference, newValue)
                }
            }
        }
    }
}
