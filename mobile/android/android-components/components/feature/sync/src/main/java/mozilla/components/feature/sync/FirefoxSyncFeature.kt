/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.sync

import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.async
import mozilla.components.concept.storage.SyncError
import mozilla.components.concept.storage.SyncableStore
import mozilla.components.service.fxa.FirefoxAccountShaped
import mozilla.components.support.base.log.logger.Logger
import kotlin.coroutines.CoroutineContext

/**
 * A feature implementation which orchestrates data synchronization of a set of [SyncableStore] which
 * all share a common [AuthType].
 *
 * [AuthType] provides us with a layer of indirection that allows consumers of [FirefoxSyncFeature]
 * to use entirely different types of [SyncableStore], without this feature needing to depend on
 * their specific implementations. Those implementations might have heavy native dependencies
 * (e.g. places and logins depend on native libraries), and we do not want to force a consumer which
 * only cares about syncing logins to have to import a places native library.
 *
 * @param coroutineContext A [CoroutineContext] that will be used to perform synchronization work.
 * @param reifyAuth A conversion method which reifies a generic [FxaAuthInfo] into an object of
 * type [AuthType].
 */
class FirefoxSyncFeature<AuthType>(
    private val coroutineContext: CoroutineContext,
    private val reifyAuth: suspend (authInfo: FxaAuthInfo) -> AuthType
) {
    private val logger = Logger("feature-sync")
    private val scope by lazy { CoroutineScope(coroutineContext) }

    private val syncableStores = mutableMapOf<String, SyncableStore<AuthType>>()

    /**
     * Adds a [SyncableStore] instance to a set of those that will be synchronized via [sync].
     *
     * @param name Name of the store; used for status reporting as part of [SyncResult].
     * @param store An instance of a [SyncableStore].
     */
    fun addSyncable(name: String, store: SyncableStore<AuthType>) = synchronized(syncableStores) {
        syncableStores[name] = store
    }

    /**
     * Performs a sync of configured [SyncableStore] history instance.
     *
     * @param account [FirefoxAccountShaped] for which to perform a sync.
     * @return a deferred [SyncResult] indicating result of synchronization of configured stores.
     */
    fun sync(account: FirefoxAccountShaped): Deferred<SyncResult> = synchronized(syncableStores) {
        val results = mutableMapOf<String, StoreSyncStatus>()

        if (syncableStores.isEmpty()) {
            return CompletableDeferred(results)
        }

        return scope.async {
            val reifiedAuthInfo = reifyAuth(account.authInfo())

            syncableStores.keys.forEach { storeName ->
                results[storeName] = syncStore(syncableStores[storeName]!!, storeName, reifiedAuthInfo)
            }
            results
        }
    }

    private suspend fun syncStore(
        store: SyncableStore<AuthType>,
        storeName: String,
        account: AuthType
    ): StoreSyncStatus {
        return StoreSyncStatus(store.sync(account).also {
            if (it is SyncError) {
                logger.error("Error synchronizing a $storeName store", it.exception)
            } else {
                logger.info("Synchronized $storeName store.")
            }
        })
    }
}
