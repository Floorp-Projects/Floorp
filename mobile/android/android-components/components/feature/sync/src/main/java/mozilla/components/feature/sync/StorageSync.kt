/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.sync

import android.support.annotation.VisibleForTesting
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import mozilla.components.concept.sync.AuthException
import mozilla.components.concept.sync.AuthInfo
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.concept.sync.StoreSyncStatus
import mozilla.components.concept.sync.SyncResult
import mozilla.components.concept.sync.SyncStatus
import mozilla.components.concept.sync.SyncStatusObserver
import mozilla.components.concept.sync.SyncableStore
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry

/**
 * A feature implementation which orchestrates data synchronization of a set of [SyncableStore]-s.
 */
class StorageSync(
    private val syncableStores: Map<String, SyncableStore>,
    private val syncScope: String
) : Observable<SyncStatusObserver> by ObserverRegistry() {
    private val logger = Logger("StorageSync")

    /**
     * Sync operation exposed by this feature is guarded by a mutex, ensuring that only one Sync
     * may be running at any given time.
     */
    @VisibleForTesting
    internal var syncMutex = Mutex()

    /**
     * Performs a sync of configured [SyncableStore] history instance. This method guarantees that
     * only one sync may be running at any given time.
     *
     * @param account [OAuthAccount] for which to perform a sync.
     * @return a [SyncResult] indicating result of synchronization of configured stores.
     */
    suspend fun sync(account: OAuthAccount): SyncResult = syncMutex.withLock { withListeners {
        if (syncableStores.isEmpty()) {
            return@withListeners mapOf()
        }

        val results = mutableMapOf<String, StoreSyncStatus>()

        val authInfo = try {
            account.authInfo(syncScope)
        } catch (e: AuthException) {
            syncableStores.keys.forEach { storeName ->
                results[storeName] = StoreSyncStatus(SyncStatus.Error(e))
            }
            return@withListeners results
        }

        syncableStores.keys.forEach { storeName ->
            results[storeName] = syncStore(syncableStores.getValue(storeName), storeName, authInfo)
        }

        return@withListeners results
    } }

    private suspend fun syncStore(
        store: SyncableStore,
        storeName: String,
        auth: AuthInfo
    ): StoreSyncStatus {
        return StoreSyncStatus(store.sync(auth).also {
            if (it is SyncStatus.Error) {
                logger.error("Error synchronizing a $storeName store", it.exception)
            } else {
                logger.info("Synchronized $storeName store.")
            }
        })
    }

    private suspend fun withListeners(block: suspend () -> SyncResult): SyncResult {
        notifyObservers { onStarted() }
        val result = block()
        notifyObservers { onIdle() }
        return result
    }
}
