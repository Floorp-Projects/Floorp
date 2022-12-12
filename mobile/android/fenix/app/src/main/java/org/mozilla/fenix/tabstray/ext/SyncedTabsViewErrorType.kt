/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.tabstray.ext

import android.content.Context
import androidx.navigation.NavController
import mozilla.components.feature.syncedtabs.view.SyncedTabsView
import org.mozilla.fenix.NavGraphDirections
import org.mozilla.fenix.R
import org.mozilla.fenix.components.accounts.FenixFxAEntryPoint
import org.mozilla.fenix.tabstray.syncedtabs.SyncedTabsListItem

/**
 * Converts [SyncedTabsView.ErrorType] to [SyncedTabsListItem.Error] with a lambda for ONLY
 * [SyncedTabsView.ErrorType.SYNC_UNAVAILABLE]
 *
 * @param context Context used to obtain the strings.
 * @param navController The controller used to handle any navigation necessary for error scenarios.
 */
fun SyncedTabsView.ErrorType.toSyncedTabsListItem(context: Context, navController: NavController) =
    when (this) {
        SyncedTabsView.ErrorType.MULTIPLE_DEVICES_UNAVAILABLE ->
            SyncedTabsListItem.Error(errorText = context.getString(R.string.synced_tabs_connect_another_device))

        SyncedTabsView.ErrorType.SYNC_ENGINE_UNAVAILABLE ->
            SyncedTabsListItem.Error(errorText = context.getString(R.string.synced_tabs_enable_tab_syncing))

        SyncedTabsView.ErrorType.SYNC_NEEDS_REAUTHENTICATION ->
            SyncedTabsListItem.Error(errorText = context.getString(R.string.synced_tabs_reauth))

        SyncedTabsView.ErrorType.NO_TABS_AVAILABLE ->
            SyncedTabsListItem.Error(
                errorText = context.getString(R.string.synced_tabs_no_tabs),
            )

        SyncedTabsView.ErrorType.SYNC_UNAVAILABLE ->
            SyncedTabsListItem.Error(
                errorText = context.getString(R.string.synced_tabs_sign_in_message),
                errorButton = SyncedTabsListItem.ErrorButton(
                    buttonText = context.getString(R.string.synced_tabs_sign_in_button),
                ) {
                    navController.navigate(
                        NavGraphDirections.actionGlobalTurnOnSync(
                            entrypoint = FenixFxAEntryPoint.SyncedTabsMenu,
                        ),
                    )
                },
            )
    }
