/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.autocomplete

import android.preference.PreferenceFragment
import org.mozilla.focus.R
import org.mozilla.focus.settings.SettingsFragment

class AutocompleteCustomDomainsFragment : PreferenceFragment() {
    override fun onResume() {
        super.onResume()

        val updater = activity as SettingsFragment.ActionBarUpdater
        updater.updateTitle(R.string.preference_autocomplete_subitem_customlist)
        updater.updateIcon(R.drawable.ic_back)
    }
}