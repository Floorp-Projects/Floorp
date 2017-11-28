/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.autocomplete

import android.content.Context
import android.util.AttributeSet
import org.mozilla.focus.settings.LearnMoreSwitchPreference

/**
 * Switch preference for enabling/disabling autocompletion for custom domains entered by the user.
 */
class AutocompleteCustomDomainsPreference(
        context: Context?,
        attrs: AttributeSet?
) : LearnMoreSwitchPreference(context, attrs) {
    override fun getLearnMoreUrl() = "http://www.mozilla.org"
}