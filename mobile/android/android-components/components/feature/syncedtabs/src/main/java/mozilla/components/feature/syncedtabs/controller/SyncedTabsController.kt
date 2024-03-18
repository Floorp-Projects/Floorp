/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.syncedtabs.controller

import mozilla.components.feature.syncedtabs.storage.SyncedTabsProvider
import mozilla.components.feature.syncedtabs.view.SyncedTabsView
import mozilla.components.service.fxa.manager.FxaAccountManager

/**
 * A controller for making the appropriate request for remote tabs from [SyncedTabsProvider] when the
 * [FxaAccountManager] account is in the appropriate state. The [SyncedTabsView] can then be notified.
 */
interface SyncedTabsController {
    val provider: SyncedTabsProvider
    val accountManager: FxaAccountManager
    val view: SyncedTabsView

    /**
     * Requests for remote tabs and notifies the [SyncedTabsView] when available with [SyncedTabsView.displaySyncedTabs]
     * otherwise notifies the appropriate error to [SyncedTabsView.onError].
     */
    fun refreshSyncedTabs()

    /**
     * Requests for the account on the [FxaAccountManager] to perform a sync.
     */
    fun syncAccount()
}
