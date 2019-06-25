/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa.sync

import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import mozilla.components.concept.sync.AuthException
import mozilla.components.concept.sync.AuthExceptionType
import mozilla.components.concept.sync.StoreSyncStatus
import mozilla.components.concept.sync.SyncAuthInfo
import mozilla.components.concept.sync.SyncableStore
import mozilla.components.concept.sync.SyncResult
import mozilla.components.concept.sync.SyncStatus
import mozilla.components.service.fxa.manager.authErrorRegistry
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry

/**
 * Orchestrates data synchronization of a set of [SyncableStore]-s.
 */
class StorageSync(
    private val syncableStores: Map<String, SyncableStore>
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
     * @param authInfo [SyncAuthInfo] that will be used to authenticate synchronization.
     * @return a [SyncResult] indicating result of synchronization of configured stores.
     */
    suspend fun sync(authInfo: SyncAuthInfo): SyncResult = syncMutex.withLock { withListeners {
        if (syncableStores.isEmpty()) {
            return@withListeners mapOf()
        }

        val results = mutableMapOf<String, StoreSyncStatus>()

        syncableStores.keys.forEach { storeName ->
            results[storeName] = syncStore(syncableStores.getValue(storeName), storeName, authInfo)
        }

        return@withListeners results
    } }

    private suspend fun syncStore(
        store: SyncableStore,
        storeName: String,
        auth: SyncAuthInfo
    ): StoreSyncStatus = StoreSyncStatus(store.sync(auth).also {
        if (it is SyncStatus.Error) {
            val message = it.exception.message
            // TODO hackiness of this check is temporary. Eventually underlying libraries will
            // surface reliable authentication exceptions which we can check by type.
            // See https://github.com/mozilla-mobile/android-components/issues/3322
            if (message != null && message.contains("401")) {
                logger.error("Hit an auth error during syncing")
                authErrorRegistry.notifyObservers {
                    onAuthErrorAsync(AuthException(AuthExceptionType.UNAUTHORIZED))
                }
            } else {
                logger.error("Error synchronizing a $storeName store", it.exception)
            }
        } else {
            logger.info("Synchronized $storeName store.")
        }
    })

    private suspend fun withListeners(block: suspend () -> SyncResult): SyncResult {
        notifyObservers { onStarted() }
        val result = block()
        notifyObservers { onIdle() }
        return result
    }
}
