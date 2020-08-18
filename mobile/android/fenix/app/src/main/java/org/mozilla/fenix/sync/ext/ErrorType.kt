/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.sync.ext

import androidx.annotation.StringRes
import androidx.navigation.NavController
import mozilla.components.feature.syncedtabs.view.SyncedTabsView.ErrorType
import org.mozilla.fenix.R
import org.mozilla.fenix.sync.SyncedTabsAdapter

/**
 * Converts the error type to the appropriate matching string resource for displaying to the user.
 */
fun ErrorType.toStringRes() = when (this) {
    ErrorType.MULTIPLE_DEVICES_UNAVAILABLE -> R.string.synced_tabs_connect_another_device
    ErrorType.SYNC_ENGINE_UNAVAILABLE -> R.string.synced_tabs_enable_tab_syncing
    ErrorType.SYNC_UNAVAILABLE -> R.string.synced_tabs_sign_in_message
    ErrorType.SYNC_NEEDS_REAUTHENTICATION -> R.string.synced_tabs_reauth
    ErrorType.NO_TABS_AVAILABLE -> R.string.synced_tabs_no_tabs
}

/**
 * Converts an error type to an [SyncedTabsAdapter.AdapterItem.Error].
 */
fun ErrorType.toAdapterItem(
    @StringRes stringResId: Int,
    navController: NavController? = null
) = when (this) {
    ErrorType.MULTIPLE_DEVICES_UNAVAILABLE,
    ErrorType.SYNC_ENGINE_UNAVAILABLE,
    ErrorType.SYNC_NEEDS_REAUTHENTICATION,
    ErrorType.NO_TABS_AVAILABLE -> SyncedTabsAdapter.AdapterItem
        .Error(descriptionResId = stringResId)
    ErrorType.SYNC_UNAVAILABLE -> SyncedTabsAdapter.AdapterItem
        .Error(descriptionResId = stringResId, navController = navController)
}
