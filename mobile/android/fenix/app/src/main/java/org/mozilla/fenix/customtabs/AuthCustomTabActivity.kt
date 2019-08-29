/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.customtabs

import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.OAuthAccount
import org.mozilla.fenix.ext.components

/**
 * A special custom tab for signing into a Firefox Account. The activity is closed once the user is signed in.
 */
class AuthCustomTabActivity : CustomTabActivity() {

    private val accountStateObserver = object : AccountObserver {
        /**
         * Navigate away from this activity when we have successful authentication
         */
        override fun onAuthenticated(account: OAuthAccount, newAccount: Boolean) {
            finish()
        }
    }

    override fun onResume() {
        super.onResume()
        val accountManager = components.backgroundServices.accountManager
        accountManager.register(accountStateObserver, this, true)
    }
}
