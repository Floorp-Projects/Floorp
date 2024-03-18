/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.recenttabs.interactor

import org.mozilla.fenix.home.recenttabs.RecentTab

/**
 * Interface for recent tab related actions in the Home screen.
 */
interface RecentTabInteractor {
    /**
     * Opens the given tab. Called when a user clicks on a recent tab.
     *
     * @param tabId The ID of the tab to open.
     */
    fun onRecentTabClicked(tabId: String)

    /**
     * Show the tabs tray. Called when a user clicks on the "Show all" button besides the recent
     * tabs.
     */
    fun onRecentTabShowAllClicked()

    /**
     * Removes a bookmark from the recent bookmark list. Called when a user clicks the "Remove"
     * button for recently saved bookmarks on the home screen.
     *
     * @param tab The tab that has been removed.
     */
    fun onRemoveRecentTab(tab: RecentTab.Tab)
}
