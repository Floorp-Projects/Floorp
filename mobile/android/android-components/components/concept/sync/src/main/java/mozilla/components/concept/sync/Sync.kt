/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.sync

import mozilla.components.support.base.observer.Observable
import java.lang.Exception

/**
 * Results of running a sync via [SyncableStore.sync].
 */
sealed class SyncStatus {
    /**
     * Sync succeeded successfully.
     */
    object Ok : SyncStatus()

    /**
     * Sync completed with an error.
     */
    data class Error(val exception: Exception) : SyncStatus()
}

/**
 * Describes a "sync" entry point for a storage layer.
 */
interface SyncableStore {
    /**
     * Performs a sync.
     *
     * @param authInfo Auth information necessary for syncing this store.
     * @return [SyncStatus] A status object describing how sync went.
     */
    suspend fun sync(authInfo: AuthInfo): SyncStatus
}

/**
 * Describes a "sync" entry point for an application.
 */
interface SyncManager : Observable<SyncStatusObserver> {
    /**
     * An authenticated account is now available.
     */
    fun authenticated(account: OAuthAccount)

    /**
     * Previously authenticated account has been logged-out.
     */
    fun loggedOut()

    /**
     * Add a store, by [name], to a set of synchronization stores.
     * Implementation is expected to be able to either access or instantiate concrete
     * [SyncableStore] instances given this name.
     */
    fun addStore(name: String)

    /**
     * Remove a store previously specified via [addStore] from a set of synchronization stores.
     */
    fun removeStore(name: String)

    /**
     * Request an immediate synchronization of all configured stores.
     *
     * @param startup Boolean flag indicating if sync is being requested in a startup situation.
     */
    fun syncNow(startup: Boolean = false)

    /**
     * Indicates if synchronization is currently active.
     */
    fun isSyncRunning(): Boolean
}

/**
 * An interface for consumers that wish to observer "sync lifecycle" events.
 */
interface SyncStatusObserver {
    /**
     * Gets called at the start of a sync, before any configured syncable is synchronized.
     */
    fun onStarted()

    /**
     * Gets called at the end of a sync, after every configured syncable has been synchronized.
     */
    fun onIdle()

    /**
     * Gets called if sync encounters an error that's worthy of processing by status observers.
     * @param error Optional relevant exception.
     */
    fun onError(error: Exception?)
}

/**
 * A set of results of running a sync operation for multiple instances of [SyncableStore].
 */
typealias SyncResult = Map<String, StoreSyncStatus>
data class StoreSyncStatus(val status: SyncStatus)
