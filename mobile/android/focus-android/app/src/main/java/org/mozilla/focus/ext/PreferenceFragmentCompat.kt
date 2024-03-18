/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.ext

import androidx.annotation.StringRes
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat

/**
 * Find a preference with the corresponding key and throw if it does not exist.
 * @param preferenceId Resource ID from preference_keys
 */
fun <T : Preference> PreferenceFragmentCompat.requirePreference(@StringRes preferenceId: Int) =
    requireNotNull(findPreference<T>(getPreferenceKey(preferenceId)))
