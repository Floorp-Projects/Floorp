/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.creditcards

import android.os.Bundle
import androidx.preference.PreferenceFragmentCompat
import org.mozilla.fenix.R
import org.mozilla.fenix.ext.showToolbar

/**
 * "Credit cards" settings fragment displays a list of settings related to autofilling, adding and
 * syncing credit cards.
 */
class CreditCardsSettingFragment : PreferenceFragmentCompat() {

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        setPreferencesFromResource(R.xml.credit_cards_preferences, rootKey)
    }

    override fun onResume() {
        super.onResume()

        showToolbar(getString(R.string.preferences_credit_cards))
    }
}
