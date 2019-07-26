/* This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.settings.account

import mozilla.components.lib.state.Action
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store

/**
 * The [Store] for holding the [AccountSettingsState] and applying [AccountAction]s.
 */
class AccountSettingsStore(
    initialState: AccountSettingsState
) : Store<AccountSettingsState, AccountSettingsAction>(
    initialState,
    ::accountStateReducer
)

sealed class LastSyncTime {
    object Never : LastSyncTime()
    data class Failed(val lastSync: Long) : LastSyncTime()
    data class Success(val lastSync: Long) : LastSyncTime()
}

/**
 * The state for the Account Settings Screen
 */
data class AccountSettingsState(
    val lastSyncedDate: LastSyncTime,
    val deviceName: String
) : State

/**
 * Actions to dispatch through the `SearchStore` to modify `SearchState` through the reducer.
 */
sealed class AccountSettingsAction : Action {
    data class SyncFailed(val time: Long) : AccountSettingsAction()
    data class SyncEnded(val time: Long) : AccountSettingsAction()
    data class UpdateDeviceName(val name: String) : AccountSettingsAction()
}

/**
 * The SearchState Reducer.
 */
fun accountStateReducer(state: AccountSettingsState, action: AccountSettingsAction): AccountSettingsState {
    return when (action) {
        is AccountSettingsAction.SyncFailed -> state.copy(lastSyncedDate = LastSyncTime.Failed(action.time))
        is AccountSettingsAction.SyncEnded -> state.copy(lastSyncedDate = LastSyncTime.Success(action.time))
        is AccountSettingsAction.UpdateDeviceName -> state.copy(deviceName = action.name)
    }
}
