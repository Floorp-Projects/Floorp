/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recentsyncedtabs.interactor

import org.mozilla.fenix.home.recentsyncedtabs.RecentSyncedTab

/**
 * Interface for recent synced tab related actions in the Home screen.
 */
interface RecentSyncedTabInteractor {
    /**
     * Opens the synced tab locally. Called when a user clicks on a recent synced tab.
     *
     * @param tab The recent synced tab that has been clicked.
     */
    fun onRecentSyncedTabClicked(tab: RecentSyncedTab)

    /**
     * Opens the tabs tray to the synced tab page. Called when a user clicks on the "See all synced
     * tabs" button.
     */
    fun onSyncedTabShowAllClicked()
}
