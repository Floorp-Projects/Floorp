/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import org.jetbrains.anko.support.v4.defaultSharedPreferences
import org.mozilla.fenix.R

class SitePermissionsFragment : PreferenceFragmentCompat() {

    private lateinit var categoryPhoneFeatures: Preference
    private lateinit var radioRecommendSettings: RadioButtonPreference
    private lateinit var radioCustomSettings: RadioButtonPreference

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        (activity as AppCompatActivity).title = getString(R.string.preferences_site_permissions)
        (activity as AppCompatActivity).supportActionBar?.show()
    }

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.site_permissions_preferences, rootKey)
    }

    override fun onResume() {
        super.onResume()
        setupPreferences()
    }

    private fun setupPreferences() {

        bindRadioRecommendedSettings()

        bindRadioCustomSettings()

        bindCategoryPhoneFeatures()

        setupRadioGroups()
    }

    private fun setupRadioGroups() {
        radioRecommendSettings.addToRadioGroup(radioCustomSettings)
        radioCustomSettings.addToRadioGroup(radioRecommendSettings)
    }

    private fun bindRadioCustomSettings() {
        val keyCustomSettings = getString(R.string.pref_key_custom_settings)
        radioCustomSettings = requireNotNull(findPreference(keyCustomSettings))

        radioCustomSettings.onClickListener {
            toggleCategoryPhoneFeatureVisibility()
        }
    }

    private fun bindRadioRecommendedSettings() {
        val keyRecommendSettings = getString(R.string.pref_key_recommended_settings)
        radioRecommendSettings = requireNotNull(findPreference(keyRecommendSettings))

        radioRecommendSettings.onClickListener {
            toggleCategoryPhoneFeatureVisibility()
        }
    }

    private fun bindCategoryPhoneFeatures() {
        val keyCategoryPhoneFeatures = getString(R.string.pref_key_category_phone_feature)

        categoryPhoneFeatures = requireNotNull(findPreference(keyCategoryPhoneFeatures))

        val isCategoryActivate = defaultSharedPreferences.getBoolean(radioCustomSettings.key, false)
        if (isCategoryActivate) {
            categoryPhoneFeatures.isVisible = true
        }
    }

    private fun toggleCategoryPhoneFeatureVisibility() {
        categoryPhoneFeatures.isVisible = !categoryPhoneFeatures.isVisible
    }
}
