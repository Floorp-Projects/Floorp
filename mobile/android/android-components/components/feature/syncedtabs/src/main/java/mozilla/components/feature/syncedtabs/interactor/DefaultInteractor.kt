/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.syncedtabs.interactor

import mozilla.components.browser.storage.sync.Tab
import mozilla.components.feature.syncedtabs.controller.SyncedTabsController
import mozilla.components.feature.syncedtabs.view.SyncedTabsView

internal class DefaultInteractor(
    override val controller: SyncedTabsController,
    override val view: SyncedTabsView,
    override val tabClicked: (Tab) -> Unit,
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
        controller.syncAccount()
    }
}
