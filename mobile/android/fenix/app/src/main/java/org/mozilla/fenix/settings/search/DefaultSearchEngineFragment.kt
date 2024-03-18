/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.search

import android.os.Bundle
import androidx.fragment.app.Fragment
import androidx.navigation.fragment.findNavController
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import mozilla.components.support.ktx.android.view.hideKeyboard
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.getPreferenceKey
import org.mozilla.fenix.ext.showToolbar

/**
 * A [Fragment] that allows user to set the default search engine.
 */
class DefaultSearchEngineFragment : PreferenceFragmentCompat() {

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.default_search_engine_preferences, rootKey)
    }

    override fun onResume() {
        super.onResume()
        view?.hideKeyboard()
        showToolbar(getString(R.string.preferences_default_search_engine))
    }

    override fun onPreferenceTreeClick(preference: Preference): Boolean {
        when (preference.key) {
            getPreferenceKey(R.string.pref_key_add_search_engine) -> {
                val directions = DefaultSearchEngineFragmentDirections
                    .actionDefaultEngineFragmentToSaveSearchEngineFragment(null)
                findNavController().navigate(directions)
            }
        }

        return super.onPreferenceTreeClick(preference)
    }
}
