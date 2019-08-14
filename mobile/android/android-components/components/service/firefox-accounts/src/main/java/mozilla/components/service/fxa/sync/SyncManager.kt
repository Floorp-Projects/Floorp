/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa.sync

import mozilla.components.concept.sync.SyncableStore
import mozilla.components.service.fxa.SyncConfig
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry
import java.io.Closeable
import java.lang.Exception
import java.util.concurrent.TimeUnit

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
 * A singleton registry of [SyncableStore] objects. [WorkManagerSyncDispatcher] will use this to
 * access configured [SyncableStore] instances.
 *
 * This pattern provides a safe way for library-defined background workers to access globally
 * available instances of stores within an application.
 */
object GlobalSyncableStoreProvider {
    private val stores: MutableMap<String, SyncableStore> = mutableMapOf()

    fun configureStore(storePair: Pair<String, SyncableStore>) {
        stores[storePair.first] = storePair.second
    }

    fun getStore(name: String): SyncableStore? {
        return stores[name]
    }
}

/**
 * Internal interface to enable testing SyncManager implementations independently from SyncDispatcher.
 */
interface SyncDispatcher : Closeable, Observable<SyncStatusObserver> {
    fun isSyncActive(): Boolean
    fun syncNow(startup: Boolean = false, debounce: Boolean = false)
    fun startPeriodicSync(unit: TimeUnit, period: Long)
    fun stopPeriodicSync()
    fun workersStateChanged(isRunning: Boolean)
}

/**
 * A base sync manager implementation.
 * @param syncConfig A [SyncConfig] object describing how sync should behave.
 */
@SuppressWarnings("TooManyFunctions")
abstract class SyncManager(
    private val syncConfig: SyncConfig
) {
    companion object {
        // Periodically sync in the background, to make our syncs a little more incremental.
        // This isn't strictly necessary, and could be considered an optimization.
        //
        // Assuming that we synchronize during app startup, our trade-offs are:
        // - not syncing in the background at all might mean longer syncs, more arduous startup syncs
        // - on a slow mobile network, these delays could be significant
        // - a delay during startup sync may affect user experience, since our data will be stale
        // for longer
        // - however, background syncing eats up some of the device resources
        // - ... so we only do so a few times per day
        // - we also rely on the OS and the WorkManager APIs to minimize those costs. It promises to
        // bundle together tasks from different applications that have similar resource-consumption
        // profiles. Specifically, we need device radio to be ON to talk to our servers; OS will run
        // our periodic syncs bundled with another tasks that also need radio to be ON, thus "spreading"
        // the overall cost.
        //
        // If we wanted to be very fancy, this period could be driven by how much new activity an
        // account is actually expected to generate. For now, it's just a hard-coded constant.
        val SYNC_PERIOD_UNIT = TimeUnit.MINUTES
    }

    open val logger = Logger("SyncManager")

    // A SyncDispatcher instance bound to an account and a set of syncable stores.
    private var syncDispatcher: SyncDispatcher? = null

    private val syncStatusObserverRegistry = ObserverRegistry<SyncStatusObserver>()

    // Manager encapsulates events emitted by the wrapped observers, and passes them on to the outside world.
    // This split allows us to define a nice public observer API (manager -> consumers), along with
    // a more robust internal observer API (dispatcher -> manager).
    // Currently the interfaces are the same, hence the name "pass-through".
    private val dispatcherStatusObserver = PassThroughSyncStatusObserver(syncStatusObserverRegistry)

    /**
     * Indicates if sync is currently running.
     */
    internal fun isSyncActive() = syncDispatcher?.isSyncActive() ?: false

    internal fun registerSyncStatusObserver(observer: SyncStatusObserver) {
        syncStatusObserverRegistry.register(observer)
    }

    /**
     * Request an immediate synchronization of all configured stores.
     *
     * @param startup Boolean flag indicating if sync is being requested in a startup situation.
     */
    internal fun now(startup: Boolean = false, debounce: Boolean = false) = synchronized(this) {
        if (syncDispatcher == null) {
            logger.info("Sync is not enabled. Ignoring 'sync now' request.")
        }
        syncDispatcher?.let {
            logger.debug("Requesting immediate sync")
            it.syncNow(startup, debounce)
        }
    }

    /**
     * Enables synchronization, with behaviour described in [syncConfig].
     */
    internal fun start() = synchronized(this) {
        logger.debug("Enabling...")
        syncDispatcher = initDispatcher(newDispatcher(syncDispatcher, syncConfig.syncableStores))
        logger.debug("set and initialized new dispatcher: $syncDispatcher")
    }

    /**
     * Disables synchronization.
     */
    internal fun stop() = synchronized(this) {
        logger.debug("Disabling...")
        syncDispatcher?.unregister(dispatcherStatusObserver)
        syncDispatcher?.stopPeriodicSync()
        syncDispatcher?.close()
        syncDispatcher = null
    }

    internal abstract fun createDispatcher(stores: Set<String>): SyncDispatcher

    internal abstract fun dispatcherUpdated(dispatcher: SyncDispatcher)

    private fun newDispatcher(
        currentDispatcher: SyncDispatcher?,
        stores: Set<String>
    ): SyncDispatcher {
        // Let the existing dispatcher, if present, cleanup.
        currentDispatcher?.close()
        // TODO will events from old and new dispatchers overlap..? How do we ensure correct sequence
        // for outside observers?
        currentDispatcher?.unregister(dispatcherStatusObserver)

        // Create a new dispatcher bound to current stores and account.
        return createDispatcher(stores)
    }

    private fun initDispatcher(dispatcher: SyncDispatcher): SyncDispatcher {
        dispatcher.register(dispatcherStatusObserver)
        dispatcher.syncNow()
        if (syncConfig.syncPeriodInMinutes != null) {
            dispatcher.startPeriodicSync(SYNC_PERIOD_UNIT, syncConfig.syncPeriodInMinutes)
        }
        dispatcherUpdated(dispatcher)
        return dispatcher
    }

    private class PassThroughSyncStatusObserver(
        private val passThroughRegistry: ObserverRegistry<SyncStatusObserver>
    ) : SyncStatusObserver {
        override fun onStarted() {
            passThroughRegistry.notifyObservers { onStarted() }
        }

        override fun onIdle() {
            passThroughRegistry.notifyObservers { onIdle() }
        }

        override fun onError(error: Exception?) {
            passThroughRegistry.notifyObservers { onError(error) }
        }
    }
}
