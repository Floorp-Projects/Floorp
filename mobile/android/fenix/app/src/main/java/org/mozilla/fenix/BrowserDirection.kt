/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import androidx.annotation.IdRes

/**
 * Used with [HomeActivity.openToBrowser] to indicate which fragment
 * the browser is being opened from.
 *
 * @property fragmentId ID of the fragment opening the browser in the navigation graph.
 * An ID of `0` indicates a global action with no corresponding opening fragment.
 */
enum class BrowserDirection(@IdRes val fragmentId: Int) {
    FromGlobal(0),
    FromHome(R.id.homeFragment),
    FromSearch(R.id.searchFragment),
    FromSearchDialog(R.id.searchDialogFragment),
    FromSettings(R.id.settingsFragment),
    FromSyncedTabs(R.id.syncedTabsFragment),
    FromBookmarks(R.id.bookmarkFragment),
    FromHistory(R.id.historyFragment),
    FromTrackingProtectionExceptions(R.id.trackingProtectionExceptionsFragment),
    FromAbout(R.id.aboutFragment),
    FromTrackingProtection(R.id.trackingProtectionFragment),
    FromSavedLoginsFragment(R.id.savedLoginsFragment),
    FromAddNewDeviceFragment(R.id.addNewDeviceFragment),
    FromAddSearchEngineFragment(R.id.addSearchEngineFragment),
    FromEditCustomSearchEngineFragment(R.id.editCustomSearchEngineFragment),
    FromAddonDetailsFragment(R.id.addonDetailsFragment),
    FromAddonPermissionsDetailsFragment(R.id.addonPermissionsDetailFragment),
    FromLoginDetailFragment(R.id.loginDetailFragment),
    FromTabTray(R.id.tabTrayDialogFragment)
}
