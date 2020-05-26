/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.sync

import android.content.Context
import androidx.lifecycle.ProcessLifecycleOwner
import mozilla.components.concept.sync.AccountObserver
import mozilla.components.concept.sync.AuthType
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.service.fxa.manager.FxaAccountManager
import org.mozilla.fenix.ext.components

class SyncedTabsIntegration(
    private val context: Context,
    private val accountManager: FxaAccountManager
) {
    fun launch() {
        val accountObserver = SyncedTabsAccountObserver(context)

        accountManager.register(
            accountObserver,
            owner = ProcessLifecycleOwner.get(),
            autoPause = true
        )
    }
}

internal class SyncedTabsAccountObserver(private val context: Context) : AccountObserver {
    override fun onAuthenticated(account: OAuthAccount, authType: AuthType) {
        context.components.backgroundServices.syncedTabsStorage.start()
    }

    override fun onLoggedOut() {
        context.components.backgroundServices.syncedTabsStorage.stop()
    }
}
