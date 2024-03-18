/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.settings

import android.content.Context
import android.util.AttributeSet
import android.widget.TextView
import androidx.preference.Preference
import androidx.preference.PreferenceViewHolder
import org.mozilla.focus.R
import org.mozilla.focus.ext.settings

/**
 * State preference that will show the current state as a summary and a sub screen to configure the behavior.
 */
class StatePreference(context: Context, attrs: AttributeSet?) : Preference(context, attrs) {
    private var summaryView: TextView? = null

    override fun onBindViewHolder(holder: PreferenceViewHolder) {
        super.onBindViewHolder(holder)

        summaryView = holder.findViewById(android.R.id.summary) as TextView
        setValueByKey(key)
    }

    private fun setValueByKey(key: String?) {
        key?.let {
            val state = when (key) {
                context.getString(R.string.pref_key_studies) -> context.settings.isExperimentationEnabled

                context.getString(R.string.pref_key_screen_autocomplete) ->
                    context.settings.shouldAutocompleteFromShippedDomainList() ||
                        context.settings.shouldAutocompleteFromCustomDomainList()
                else -> false
            }
            val summaryText = if (state) {
                R.string.preference_state_on
            } else {
                R.string.preference_state_off
            }

            summaryView?.setText(summaryText)
        }
    }
}
