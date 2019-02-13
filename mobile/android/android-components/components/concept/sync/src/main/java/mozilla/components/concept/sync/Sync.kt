/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.sync

import mozilla.components.support.base.observer.Observable
import java.io.Closeable
import java.lang.Exception

interface SyncManager : Observable<SyncStatusObserver> {
    fun authenticated(account: OAuthAccount)
    fun loggedOut()

    fun addStore(name: String, store: SyncableStore)

    fun removeStore(name: String)

    /**
     * Kick-off an immediate sync.
     *
     * @param startup Boolean flag indicating if sync is being requested in a startup situation.
     */
    fun syncNow(startup: Boolean = false)

    fun createDispatcher(stores: Map<String, SyncableStore>, account: OAuthAccount): SyncDispatcher
}

interface SyncDispatcher : Closeable, Observable<SyncStatusObserver> {
    fun isSyncActive(): Boolean

    /**
     * Kick-off an immediate sync.
     *
     * @param startup Boolean flag indicating if sync is being requested in a startup situation.
     */
    fun syncNow(startup: Boolean = false)

    fun startPeriodicSync()

    fun stopPeriodicSync()
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
