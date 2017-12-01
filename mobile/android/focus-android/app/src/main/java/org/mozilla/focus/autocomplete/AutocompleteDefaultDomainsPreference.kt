/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.autocomplete

import android.content.Context
import android.util.AttributeSet
import org.mozilla.focus.R
import org.mozilla.focus.settings.LearnMoreSwitchPreference
import org.mozilla.focus.utils.SupportUtils

/**
 * Switch preference for enabling/disabling autocompletion for default domains that ship with the app.
 */
class AutocompleteDefaultDomainsPreference(
        context: Context?,
        attrs: AttributeSet?
) : LearnMoreSwitchPreference(context, attrs) {
    override fun getLearnMoreUrl() = SupportUtils.getAutocompleteUrl(context)

    override fun getDescription(): String? =
            context.getString(R.string.preference_autocomplete_preinstalled_summary,
                    context.getString(R.string.app_name))
}
