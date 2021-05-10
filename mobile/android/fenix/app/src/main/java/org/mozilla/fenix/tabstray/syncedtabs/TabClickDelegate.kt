/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.syncedtabs

import mozilla.components.browser.storage.sync.Tab
import mozilla.components.feature.syncedtabs.view.SyncedTabsView
import org.mozilla.fenix.tabstray.NavigationInteractor

/**
 * A wrapper class that handles tab clicks from a Synced Tabs list.
 */
class TabClickDelegate(
    private val interactor: NavigationInteractor
) : SyncedTabsView.Listener {
    override fun onTabClicked(tab: Tab) {
        interactor.onSyncedTabClicked(tab)
    }

    override fun onRefresh() = Unit
}
