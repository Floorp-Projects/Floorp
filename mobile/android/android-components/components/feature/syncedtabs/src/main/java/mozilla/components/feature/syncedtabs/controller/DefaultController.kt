/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.syncedtabs.controller

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import mozilla.components.feature.syncedtabs.storage.SyncedTabsProvider
import mozilla.components.feature.syncedtabs.view.SyncedTabsView
import mozilla.components.feature.syncedtabs.view.SyncedTabsView.ErrorType
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.fxa.manager.ext.withConstellation
import mozilla.components.service.fxa.sync.SyncReason
import kotlin.coroutines.CoroutineContext

internal class DefaultController(
    override val provider: SyncedTabsProvider,
    override val accountManager: FxaAccountManager,
    override val view: SyncedTabsView,
    coroutineContext: CoroutineContext
) : SyncedTabsController {

    private val scope = CoroutineScope(coroutineContext)

    /**
     * See [SyncedTabsController.refreshSyncedTabs]
     */
    override fun refreshSyncedTabs() {
        view.startLoading()

        scope.launch {
            accountManager.withConstellation {
                val syncedTabs = provider.getSyncedTabs()
                val otherDevices = state()?.otherDevices

                scope.launch(Dispatchers.Main) {
                    if (syncedTabs.isEmpty() && otherDevices?.isEmpty() == true) {
                        view.onError(ErrorType.MULTIPLE_DEVICES_UNAVAILABLE)
                    } else {
                        view.displaySyncedTabs(syncedTabs)
                    }
                }
            }

            scope.launch(Dispatchers.Main) {
                view.stopLoading()
            }
        }
    }

    /**
     * See [SyncedTabsController.syncAccount]
     */
    override fun syncAccount() {
        scope.launch {
            accountManager.withConstellation {
                refreshDevicesAsync().await()
            }
            accountManager.syncNowAsync(SyncReason.User)
        }
    }
}
