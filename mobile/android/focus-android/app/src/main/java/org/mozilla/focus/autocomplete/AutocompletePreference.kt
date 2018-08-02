/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.autocomplete

import android.content.Context
import android.support.v7.preference.Preference
import android.support.v7.preference.PreferenceViewHolder
import android.util.AttributeSet
import android.widget.TextView
import org.mozilla.focus.R
import org.mozilla.focus.utils.Settings

/**
 * Autocomplete preference that will show a sub screen to configure the autocomplete behavior.
 */
class AutocompletePreference(context: Context?, attrs: AttributeSet?) : Preference(context, attrs) {
    var summaryView: TextView? = null

    override fun onBindViewHolder(holder: PreferenceViewHolder?) {
        super.onBindViewHolder(holder)

        val settings = Settings.getInstance(context)

        summaryView = holder!!.findViewById(android.R.id.summary) as TextView
        summaryView?.setText(
            if (settings.shouldAutocompleteFromShippedDomainList() ||
                settings.shouldAutocompleteFromCustomDomainList())
                R.string.preference_state_on
            else
                R.string.preference_state_off)
    }
}
