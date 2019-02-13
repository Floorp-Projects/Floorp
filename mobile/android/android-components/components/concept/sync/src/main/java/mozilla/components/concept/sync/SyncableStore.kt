/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.sync

import java.lang.Exception

/**
 * Describes a "sync" entry point for store which operates over [AuthInfo].
 */
interface SyncableStore<AuthInfo> {
    /**
     * Performs a sync.
     * @param authInfo Auth information of type [AuthInfo] necessary for syncing this store.
     * @return [SyncStatus] A status object describing how sync went.
     */
    suspend fun sync(authInfo: AuthInfo): SyncStatus
}

sealed class SyncStatus
object SyncOk : SyncStatus()
data class SyncError(val exception: Exception) : SyncStatus()
