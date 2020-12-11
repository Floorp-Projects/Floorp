/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.os.Bundle
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import com.leanplum.Leanplum
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.showToolbar

class SecretDebugSettingsFragment : PreferenceFragmentCompat() {

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.preferences_debug_info))
    }

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.secret_info_settings_preferences, rootKey)

        requirePreference<Preference>(R.string.pref_key_leanplum_user_id).apply {
            summary = Leanplum.getUserId().let {
                if (it.isNullOrEmpty()) {
                    "No User Id"
                } else {
                    it
                }
            }
        }

        requirePreference<Preference>(R.string.pref_key_leanplum_device_id).apply {
            summary = Leanplum.getDeviceId().let {
                if (it.isNullOrEmpty()) {
                    "No Device Id"
                } else {
                    it
                }
            }
        }
    }
}
