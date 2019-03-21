/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import androidx.preference.PreferenceFragmentCompat
import org.mozilla.fenix.R

class SitePermissionsFragment : PreferenceFragmentCompat() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        (activity as AppCompatActivity).title = getString(R.string.preferences_site_permissions)
        (activity as AppCompatActivity).supportActionBar?.show()
    }

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.site_permissions_preferences, rootKey)
    }

    private fun setupPreferences() {
        val keyRecommendSettings = getString(R.string.pref_key_recommended_settings)
        val keyCustomSettings = getString(R.string.pref_key_custom_settings)
        val radioRecommendSettings: RadioButtonPreference = requireNotNull(findPreference(keyRecommendSettings))
        val radioCustomSettings: RadioButtonPreference = requireNotNull(findPreference(keyCustomSettings))

        radioRecommendSettings.addToRadioGroup(radioCustomSettings)
        radioCustomSettings.addToRadioGroup(radioRecommendSettings)
    }

    override fun onResume() {
        super.onResume()
        setupPreferences()
    }
}
