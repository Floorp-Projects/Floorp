/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.account

import mozilla.components.lib.state.Action
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store

/**
 * The [Store] for holding the [AccountSettingsFragmentState] and applying [AccountAction]s.
 */
class AccountSettingsFragmentStore(
    initialState: AccountSettingsFragmentState,
) : Store<AccountSettingsFragmentState, AccountSettingsFragmentAction>(
    initialState,
    ::accountStateReducer,
)

sealed class LastSyncTime {
    object Never : LastSyncTime()
    data class Failed(val lastSync: Long) : LastSyncTime()
    data class Success(val lastSync: Long) : LastSyncTime()
}

/**
 * The state for the Account Settings Screen
 */
data class AccountSettingsFragmentState(
    val lastSyncedDate: LastSyncTime = LastSyncTime.Never,
    val deviceName: String = "",
) : State

/**
 * Actions to dispatch through the `SearchStore` to modify `SearchState` through the reducer.
 */
sealed class AccountSettingsFragmentAction : Action {
    data class SyncFailed(val time: Long) : AccountSettingsFragmentAction()
    data class SyncEnded(val time: Long) : AccountSettingsFragmentAction()
    data class UpdateDeviceName(val name: String) : AccountSettingsFragmentAction()
}

/**
 * The SearchState Reducer.
 */
private fun accountStateReducer(
    state: AccountSettingsFragmentState,
    action: AccountSettingsFragmentAction,
): AccountSettingsFragmentState {
    return when (action) {
        is AccountSettingsFragmentAction.SyncFailed -> state.copy(lastSyncedDate = LastSyncTime.Failed(action.time))
        is AccountSettingsFragmentAction.SyncEnded -> state.copy(lastSyncedDate = LastSyncTime.Success(action.time))
        is AccountSettingsFragmentAction.UpdateDeviceName -> state.copy(deviceName = action.name)
    }
}
