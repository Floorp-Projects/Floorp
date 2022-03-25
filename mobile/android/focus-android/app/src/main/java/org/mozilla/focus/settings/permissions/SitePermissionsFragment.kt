/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.focus.settings.permissions

import android.os.Bundle
import androidx.preference.Preference
import org.mozilla.focus.R
import org.mozilla.focus.ext.requireComponents
import org.mozilla.focus.ext.requirePreference
import org.mozilla.focus.settings.BaseSettingsFragment
import org.mozilla.focus.settings.permissions.permissionoptions.SitePermission
import org.mozilla.focus.settings.permissions.permissionoptions.SitePermissionOptionsStorage
import org.mozilla.focus.state.AppAction

class SitePermissionsFragment : BaseSettingsFragment() {

    override fun onStart() {
        super.onStart()
        updateTitle(R.string.preference_site_permissions)
    }

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.site_permissions, rootKey)
        bindCategoryPhoneFeatures()
    }

    private fun bindCategoryPhoneFeatures() {
        SitePermission.values()
            // Only AUTOPLAY should appear in the list AUTOPLAY_INAUDIBLE and AUTOPLAY_AUDIBLE
            // shouldn't be bound
            .filter { it != SitePermission.AUTOPLAY_INAUDIBLE && it != SitePermission.AUTOPLAY_AUDIBLE }
            .forEach(::initPhoneFeature)
    }

    private fun initPhoneFeature(sitePermission: SitePermission) {
        val storage = SitePermissionOptionsStorage(requireContext())
        val preferencePhoneFeatures = requirePreference<Preference>(
            storage.getSitePermissionPreferenceId(sitePermission)
        )
        preferencePhoneFeatures.summary = storage.getSitePermissionOptionSelectedLabel(sitePermission)

        preferencePhoneFeatures.onPreferenceClickListener = Preference.OnPreferenceClickListener {
            navigateToSitePermissionOptionsScreen(sitePermission)
            true
        }
    }

    private fun navigateToSitePermissionOptionsScreen(sitePermission: SitePermission) {
        requireComponents.appStore.dispatch(AppAction.OpenSitePermissionOptionsScreen(sitePermission = sitePermission))
    }
}
