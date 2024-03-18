/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa.store

import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.Store

/**
 * [Store] for the global [SyncState]. This should likely be a singleton.
 */
class SyncStore(
    middleware: List<Middleware<SyncState, SyncAction>> = emptyList(),
) : Store<SyncState, SyncAction>(
    initialState = SyncState(),
    reducer = ::reduce,
    middleware = middleware,
)

private fun reduce(syncState: SyncState, syncAction: SyncAction): SyncState {
    return when (syncAction) {
        is SyncAction.UpdateSyncStatus -> syncState.copy(status = syncAction.status)
        is SyncAction.UpdateAccount -> syncState.copy(account = syncAction.account)
        is SyncAction.UpdateDeviceConstellation ->
            syncState.copy(constellationState = syncAction.deviceConstellation)
    }
}
