/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa.store

import mozilla.components.concept.sync.Avatar
import mozilla.components.concept.sync.ConstellationState
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.concept.sync.Profile
import mozilla.components.lib.state.State
import mozilla.components.service.fxa.sync.WorkManagerSyncManager

/**
 * Global state of Sync.
 *
 * @property status The current status of Sync.
 * @property account The current Sync account, if any.
 * @property constellationState The current constellation state, if any.
 */
data class SyncState(
    val status: SyncStatus = SyncStatus.NotInitialized,
    val account: Account? = null,
    val constellationState: ConstellationState? = null,
) : State

/**
 * Various statuses described the [SyncState].
 *
 * Starts as [NotInitialized].
 * Becomes [Started] during the length of a Sync.
 * Becomes [Idle] when a Sync is completed.
 * Becomes [Error] when a Sync encounters an error.
 * Becomes [LoggedOut] when Sync is logged out.
 *
 * See [WorkManagerSyncManager] for implementation details.
 */
enum class SyncStatus {
    Started,
    Idle,
    Error,
    NotInitialized,
    LoggedOut,
}

/**
 * Account information available for a synced account.
 *
 * @property uid See [Profile.uid].
 * @property email See [Profile.email].
 * @property avatar See [Profile.avatar].
 * @property displayName See [Profile.displayName].
 * @property currentDeviceId See [OAuthAccount.getCurrentDeviceId].
 * @property sessionToken See [OAuthAccount.getSessionToken].
 */
data class Account(
    val uid: String?,
    val email: String?,
    val avatar: Avatar?,
    val displayName: String?,
    val currentDeviceId: String?,
    val sessionToken: String?,
)
