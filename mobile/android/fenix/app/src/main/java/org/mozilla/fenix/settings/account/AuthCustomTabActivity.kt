/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.account

import android.os.Bundle
import android.os.PersistableBundle
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.AuthType
import mozilla.components.concept.sync.OAuthAccount
import org.mozilla.fenix.customtabs.ExternalAppBrowserActivity
import org.mozilla.fenix.ext.components
import org.mozilla.fenix.ext.enableSystemInsetsHandling

/**
 * A special custom tab for signing into a Firefox Account. The activity is closed once the user is signed in.
 */
class AuthCustomTabActivity : ExternalAppBrowserActivity() {

    override fun onCreate(savedInstanceState: Bundle?, persistentState: PersistableBundle?) {
        super.onCreate(savedInstanceState, persistentState)
        enableSystemInsetsHandling()
    }

    private val accountStateObserver = object : AccountObserver {
        /**
         * Navigate away from this activity when we have successful authentication
         */
        override fun onAuthenticated(account: OAuthAccount, authType: AuthType) {
            finish()
        }
    }

    override fun onResume() {
        super.onResume()
        val accountManager = components.backgroundServices.accountManager
        accountManager.register(accountStateObserver, this, true)
    }
}
