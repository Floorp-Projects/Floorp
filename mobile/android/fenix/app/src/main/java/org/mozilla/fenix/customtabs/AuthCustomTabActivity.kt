package org.mozilla.fenix.customtabs

/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.concept.sync.Profile
import mozilla.components.service.fxa.manager.FxaAccountManager
import org.mozilla.fenix.ext.components

class AuthCustomTabActivity : CustomTabActivity() {
    private lateinit var accountManager: FxaAccountManager

    // Navigate away from this activity when we have successful authentication
    private val accountStateObserver = object : AccountObserver {
        override fun onAuthenticated(account: OAuthAccount) {
            this@AuthCustomTabActivity.finish()
        }

        override fun onAuthenticationProblems() {}

        override fun onLoggedOut() {}

        override fun onProfileUpdated(profile: Profile) {}
    }

    override fun onResume() {
        super.onResume()
        accountManager = this.components.backgroundServices.accountManager
        accountManager.register(accountStateObserver, this, true)
    }
}
