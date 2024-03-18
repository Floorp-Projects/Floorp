/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa.store

import mozilla.components.concept.sync.ConstellationState
import mozilla.components.lib.state.Action

/**
 * Actions for updating the global [SyncState] via [SyncStore].
 */
sealed class SyncAction : Action {
    /**
     * Update the [SyncState.status] of the [SyncStore].
     */
    data class UpdateSyncStatus(val status: SyncStatus) : SyncAction()

    /**
     * Update the [SyncState.account] of the [SyncStore].
     */
    data class UpdateAccount(val account: Account?) : SyncAction()

    /**
     * Update the [SyncState.constellationState] of the [SyncStore].
     */
    data class UpdateDeviceConstellation(val deviceConstellation: ConstellationState) : SyncAction()
}
