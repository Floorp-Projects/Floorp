/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.syncedtabs.interactor

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import mozilla.components.browser.storage.sync.Tab
import mozilla.components.feature.syncedtabs.view.SyncedTabsView
import mozilla.components.service.fxa.manager.FxaAccountManager
import mozilla.components.service.fxa.manager.ext.withConstellation
import mozilla.components.service.fxa.sync.SyncReason
import kotlin.coroutines.CoroutineContext

internal class DefaultInteractor(
    override val accountManager: FxaAccountManager,
    override val view: SyncedTabsView,
    private val coroutineContext: CoroutineContext,
    override val tabClicked: (Tab) -> Unit
) : SyncedTabsInteractor {

    override fun start() {
        view.listener = this
    }

    override fun stop() {
        view.listener = null
    }

    override fun onTabClicked(tab: Tab) {
        tabClicked(tab)
    }

    override fun onRefresh() {
        CoroutineScope(coroutineContext).launch {
            accountManager.withConstellation {
                refreshDevicesAsync()
            }
            accountManager.syncNowAsync(SyncReason.User, true)
        }
    }
}
