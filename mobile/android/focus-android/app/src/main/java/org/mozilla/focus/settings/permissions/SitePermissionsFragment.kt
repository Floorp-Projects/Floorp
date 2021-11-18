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
import org.mozilla.focus.state.AppAction
import org.mozilla.focus.state.Screen

class SitePermissionsFragment : BaseSettingsFragment() {

    override fun onStart() {
        super.onStart()
        updateTitle(R.string.preference_site_permissions)
    }

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        addPreferencesFromResource(R.xml.site_permissions)
        requirePreference<Preference>(R.string.pref_key_autoplay).summary =
            getString(requireComponents.settings.currentAutoplayOption.textId)
    }

    override fun onPreferenceTreeClick(preference: Preference): Boolean {
        when (preference.key) {
            resources.getString(R.string.pref_key_autoplay) ->
                requireComponents.appStore.dispatch(
                    AppAction.OpenSettings(page = Screen.Settings.Page.Autoplay)
                )
        }
        return super.onPreferenceTreeClick(preference)
    }
}
