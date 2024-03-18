/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings.advanced

import androidx.core.content.edit
import androidx.preference.Preference
import org.mozilla.focus.ext.settings

/**
 * Updates the corresponding [android.content.SharedPreferences] when the boolean [Preference] is changed.
 * The preference key is used as the shared preference key.
 */
open class SharedPreferenceUpdater : Preference.OnPreferenceChangeListener {

    override fun onPreferenceChange(preference: Preference, newValue: Any?): Boolean {
        val newBooleanValue = newValue as? Boolean ?: return false
        preference.context.settings.preferences.edit {
            putBoolean(preference.key, newBooleanValue)
        }
        return true
    }
}
