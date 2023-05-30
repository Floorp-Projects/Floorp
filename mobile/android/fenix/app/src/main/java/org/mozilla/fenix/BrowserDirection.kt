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
    FromWallpaper(R.id.wallpaperSettingsFragment),
    FromSearchDialog(R.id.searchDialogFragment),
    FromSettings(R.id.settingsFragment),
    FromBookmarks(R.id.bookmarkFragment),
    FromBookmarkSearchDialog(R.id.bookmarkSearchDialogFragment),
    FromHistory(R.id.historyFragment),
    FromHistorySearchDialog(R.id.historySearchDialogFragment),
    FromHistoryMetadataGroup(R.id.historyMetadataGroupFragment),
    FromTrackingProtectionExceptions(R.id.trackingProtectionExceptionsFragment),
    FromAbout(R.id.aboutFragment),
    FromTrackingProtection(R.id.trackingProtectionFragment),
    FromHttpsOnlyMode(R.id.httpsOnlyFragment),
    FromCookieBanner(R.id.cookieBannerFragment),
    FromTrackingProtectionDialog(R.id.trackingProtectionPanelDialogFragment),
    FromSavedLoginsFragment(R.id.savedLoginsFragment),
    FromAddNewDeviceFragment(R.id.addNewDeviceFragment),
    FromAddSearchEngineFragment(R.id.addSearchEngineFragment),
    FromEditCustomSearchEngineFragment(R.id.editCustomSearchEngineFragment),
    FromSaveSearchEngineFragment(R.id.saveSearchEngineFragment),
    FromAddonDetailsFragment(R.id.addonDetailsFragment),
    FromStudiesFragment(R.id.studiesFragment),
    FromAddonPermissionsDetailsFragment(R.id.addonPermissionsDetailFragment),
    FromLoginDetailFragment(R.id.loginDetailFragment),
    FromTabsTray(R.id.tabsTrayFragment),
    FromRecentlyClosed(R.id.recentlyClosedFragment),
}
