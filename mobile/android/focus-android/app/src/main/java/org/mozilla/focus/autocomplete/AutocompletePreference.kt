/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.autocomplete

import android.content.Context
import android.preference.Preference
import android.util.AttributeSet
import android.view.View
import android.widget.TextView
import org.mozilla.focus.R
import org.mozilla.focus.utils.Settings

class AutocompletePreference(context: Context?, attrs: AttributeSet?) : Preference(context, attrs) {
    var summaryView : TextView? = null

    override fun onBindView(view: View?) {
        super.onBindView(view)

        val settings = Settings.getInstance(context)

        summaryView = view?.findViewById(android.R.id.summary)
        summaryView?.setText(
                if (settings.shouldAutocompleteFromShippedDomainList()
                        || settings.shouldAutocompleteFromCustomDomainList())
                    R.string.preference_state_enabled
                else
                    R.string.preference_state_disabled)
    }
}
