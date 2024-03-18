/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings

import android.content.Context
import android.util.AttributeSet
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.settings

/**
 * Custom [DropDownListPreference] that automatically builds the list of available options for the
 * custom Enhanced Tracking Protection option depending on the current Nimbus experiments.
 */
class CustomEtpCookiesOptionsDropDownListPreference @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
) : DropDownListPreference(context, attrs) {
    init {
        with(context) {
            entries = arrayOf(
                getString(R.string.preference_enhanced_tracking_protection_custom_cookies_1),
                getString(R.string.preference_enhanced_tracking_protection_custom_cookies_2),
                getString(R.string.preference_enhanced_tracking_protection_custom_cookies_3),
                getString(R.string.preference_enhanced_tracking_protection_custom_cookies_4),
            )

            entryValues = arrayOf(
                getString(R.string.social),
                getString(R.string.unvisited),
                getString(R.string.third_party),
                getString(R.string.all),
            )

            @Suppress("UNCHECKED_CAST")
            if (context.settings().enabledTotalCookieProtection) {
                // If the new "Total cookie protection" should be shown it must be first item.
                entries = arrayOf(getString(R.string.preference_enhanced_tracking_protection_custom_cookies_5)) +
                    entries as Array<String>
                entryValues = arrayOf(getString(R.string.total_protection)) + entryValues as Array<String>
            }
        }

        setDefaultValue(entryValues.first())
    }
}
