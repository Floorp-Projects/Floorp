/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import androidx.navigation.Navigation
import androidx.preference.Preference
import androidx.preference.Preference.OnPreferenceClickListener
import androidx.preference.PreferenceFragmentCompat
import org.jetbrains.anko.support.v4.defaultSharedPreferences
import org.mozilla.fenix.R
import org.mozilla.fenix.settings.SitePermissionsManagePhoneFeature.PhoneFeature
import org.mozilla.fenix.settings.SitePermissionsManagePhoneFeature.PhoneFeature.NOTIFICATION
import org.mozilla.fenix.settings.SitePermissionsManagePhoneFeature.PhoneFeature.LOCATION
import org.mozilla.fenix.settings.SitePermissionsManagePhoneFeature.PhoneFeature.CAMERA
import org.mozilla.fenix.settings.SitePermissionsManagePhoneFeature.PhoneFeature.MICROPHONE
import org.mozilla.fenix.utils.Settings

@SuppressWarnings("TooManyFunctions")
class SitePermissionsFragment : PreferenceFragmentCompat() {

    private lateinit var categoryPhoneFeatures: Preference
    private lateinit var radioRecommendSettings: RadioButtonPreference
    private lateinit var radioCustomSettings: RadioButtonPreference

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
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
            categoryPhoneFeatures.isVisible = true
        }
    }

    private fun bindRadioRecommendedSettings() {
        val keyRecommendSettings = getString(R.string.pref_key_recommended_settings)
        radioRecommendSettings = requireNotNull(findPreference(keyRecommendSettings))

        radioRecommendSettings.onClickListener {
            categoryPhoneFeatures.isVisible = false
        }
    }

    private fun bindCategoryPhoneFeatures() {
        val keyCategoryPhoneFeatures = getString(R.string.pref_key_category_phone_feature)

        categoryPhoneFeatures = requireNotNull(findPreference(keyCategoryPhoneFeatures))

        val isCategoryActivate = defaultSharedPreferences.getBoolean(radioCustomSettings.key, false)
        if (isCategoryActivate) {
            categoryPhoneFeatures.isVisible = true
        }

        val settings = Settings.getInstance(requireContext())

        val cameraAction = settings
            .getSitePermissionsPhoneFeatureCameraAction()
            .toString(requireContext())

        val locationAction = settings
            .getSitePermissionsPhoneFeatureLocation()
            .toString(requireContext())

        val microPhoneAction = settings
            .getSitePermissionsPhoneFeatureMicrophoneAction()
            .toString(requireContext())

        val notificationAction = settings
            .getSitePermissionsPhoneFeatureNotificationAction()
            .toString(requireContext())

        initPhoneFeature(CAMERA, cameraAction)
        initPhoneFeature(LOCATION, locationAction)
        initPhoneFeature(MICROPHONE, microPhoneAction)
        initPhoneFeature(NOTIFICATION, notificationAction)
    }

    private fun initPhoneFeature(phoneFeature: PhoneFeature, summary: String) {
        val keyPreference = getPreferenceKeyBy(phoneFeature)
        val cameraPhoneFeatures: Preference = requireNotNull(findPreference(keyPreference))
        cameraPhoneFeatures.summary = summary

        cameraPhoneFeatures.onPreferenceClickListener = OnPreferenceClickListener {
            navigateToPhoneFeature(phoneFeature)
            true
        }
    }

    private fun getPreferenceKeyBy(phoneFeature: PhoneFeature): String {
        return when (phoneFeature) {
            CAMERA -> getString(R.string.pref_key_phone_feature_camera)
            LOCATION -> getString(R.string.pref_key_phone_feature_location)
            MICROPHONE -> getString(R.string.pref_key_phone_feature_microphone)
            NOTIFICATION -> getString(R.string.pref_key_phone_feature_notification)
        }
    }

    private fun navigateToPhoneFeature(phoneFeature: PhoneFeature) {
        val directions = SitePermissionsFragmentDirections.actionSitePermissionsToManagePhoneFeatures(phoneFeature.id)
        Navigation.findNavController(view!!).navigate(directions)
    }
}
