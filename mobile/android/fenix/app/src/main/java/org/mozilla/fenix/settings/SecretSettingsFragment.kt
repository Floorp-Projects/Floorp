/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.os.Bundle
import androidx.lifecycle.lifecycleScope
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.SwitchPreference
import com.google.android.play.core.ktx.launchReview
import com.google.android.play.core.ktx.requestReview
import com.google.android.play.core.review.ReviewManagerFactory
import kotlinx.coroutines.launch
import org.mozilla.fenix.FeatureFlags
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.showToolbar

/**
 * Lets the user customize Private browsing options.
 */
class SecretSettingsFragment : PreferenceFragmentCompat() {

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.preferences_debug_settings))
    }

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.secret_settings_preferences, rootKey)

        requirePreference<SwitchPreference>(R.string.pref_key_enable_top_frecent_sites).apply {
            isVisible = FeatureFlags.topFrecentSite
            isChecked = context.settings().showTopFrecentSites
            onPreferenceChangeListener = SharedPreferenceUpdater()
        }

        requirePreference<SwitchPreference>(R.string.pref_key_wait_first_paint).apply {
            isVisible = FeatureFlags.waitUntilPaintToDraw
            isChecked = context.settings().waitToShowPageUntilFirstPaint
            onPreferenceChangeListener = SharedPreferenceUpdater()
        }

        requirePreference<SwitchPreference>(R.string.pref_key_synced_tabs_tabs_tray).apply {
            isVisible = FeatureFlags.syncedTabsInTabsTray
            isChecked = context.settings().syncedTabsInTabsTray
            onPreferenceChangeListener = SharedPreferenceUpdater()
        }

        requirePreference<Preference>(R.string.pref_key_temp_review_prompt).apply {
            setOnPreferenceClickListener {
                viewLifecycleOwner.lifecycleScope.launch {
                    val manager = ReviewManagerFactory.create(requireContext())
                    val reviewInfo = manager.requestReview()
                    manager.launchReview(requireActivity(), reviewInfo)
                }
                true
            }
        }
    }
}
