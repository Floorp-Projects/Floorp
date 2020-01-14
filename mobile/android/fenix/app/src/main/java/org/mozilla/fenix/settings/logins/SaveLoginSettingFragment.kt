/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.logins

import android.os.Bundle
import androidx.preference.PreferenceFragmentCompat
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.showToolbar
import org.mozilla.fenix.settings.RadioButtonPreference

class SaveLoginSettingFragment : PreferenceFragmentCompat() {
    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.save_logins_preferences, rootKey)
    }

    override fun onResume() {
        super.onResume()
        showToolbar(getString(R.string.preferences_passwords_save_logins))
        val save = bindSave()
        val neverSave = bindNeverSave()
        setupRadioGroups(save, neverSave)
    }

    private fun bindSave(): RadioButtonPreference {
        val keyStrict = getString(R.string.pref_key_save_logins)
        return requireNotNull(findPreference(keyStrict))
    }

    private fun bindNeverSave(): RadioButtonPreference {
        val keyStandard = getString(R.string.pref_key_never_save_logins)
        return requireNotNull(findPreference(keyStandard))
    }

    private fun setupRadioGroups(
        radioNeverSave: RadioButtonPreference,
        radioSave: RadioButtonPreference
    ) {
        radioNeverSave.addToRadioGroup(radioSave)
        radioSave.addToRadioGroup(radioNeverSave)
    }
}
