/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.syncedtabs

import mozilla.components.browser.storage.sync.Tab
import mozilla.components.feature.syncedtabs.view.SyncedTabsView
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.tabstray.TabsTrayController
import org.mozilla.fenix.tabstray.TabsTrayInteractor

class SyncedTabsInteractor(
    private val metrics: MetricController,
    private val activity: HomeActivity,
    private val trayInteractor: TabsTrayInteractor,
    private val controller: TabsTrayController
) : SyncedTabsView.Listener {
    override fun onRefresh() {
        controller.onSyncStarted()
    }
    override fun onTabClicked(tab: Tab) {
        metrics.track(Event.SyncedTabOpened)
        activity.openToBrowserAndLoad(
            searchTermOrURL = tab.active().url,
            newTab = true,
            from = BrowserDirection.FromTabTray
        )
        trayInteractor.navigateToBrowser()
    }
}
