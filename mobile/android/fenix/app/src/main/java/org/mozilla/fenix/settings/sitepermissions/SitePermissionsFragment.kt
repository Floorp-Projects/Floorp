/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.sitepermissions

import android.os.Bundle
import androidx.navigation.Navigation
import androidx.preference.Preference
import androidx.preference.Preference.OnPreferenceClickListener
import androidx.preference.PreferenceFragmentCompat
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.getPreferenceKey
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.settings.PhoneFeature

@SuppressWarnings("TooManyFunctions")
class SitePermissionsFragment : PreferenceFragmentCompat() {

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.site_permissions_preferences, rootKey)
    }

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.preferences_site_permissions))
        setupPreferences()
    }

    private fun setupPreferences() {
        bindCategoryPhoneFeatures()
        bindExceptions()
    }

    private fun bindExceptions() {
        val keyExceptions = getPreferenceKey(R.string.pref_key_show_site_exceptions)
        val exceptionsCategory = requireNotNull<Preference>(findPreference(keyExceptions))

        exceptionsCategory.onPreferenceClickListener = OnPreferenceClickListener {
            val directions = SitePermissionsFragmentDirections.actionSitePermissionsToExceptions()
            Navigation.findNavController(requireView()).navigate(directions)
            true
        }
    }

    private fun bindCategoryPhoneFeatures() {
        PhoneFeature.values()
            // Autoplay inaudible should be set in the same menu as autoplay audible, so it does
            // not need to be bound
            .filter { it != PhoneFeature.AUTOPLAY_INAUDIBLE }
            .forEach(::initPhoneFeature)
    }

    private fun initPhoneFeature(phoneFeature: PhoneFeature) {
        val context = requireContext()
        val settings = context.settings()

        val summary = phoneFeature.getActionLabel(context, settings = settings)
        // Remove autoplaySummary after https://bugzilla.mozilla.org/show_bug.cgi?id=1621825 is fixed
        val autoplaySummary =
            if (summary == context.getString(R.string.preference_option_autoplay_allowed2)) {
                context.getString(R.string.preference_option_autoplay_allowed_wifi_only2)
            } else {
                null
            }
        val preferenceKey = phoneFeature.getPreferenceKey(context)

        val cameraPhoneFeatures: Preference = requireNotNull(findPreference(preferenceKey))
        cameraPhoneFeatures.summary = autoplaySummary ?: summary

        cameraPhoneFeatures.onPreferenceClickListener = OnPreferenceClickListener {
            navigateToPhoneFeature(phoneFeature)
            true
        }
    }

    private fun navigateToPhoneFeature(phoneFeature: PhoneFeature) {
        val directions = SitePermissionsFragmentDirections
            .actionSitePermissionsToManagePhoneFeatures(phoneFeature)
        Navigation.findNavController(requireView()).navigate(directions)
    }
}
