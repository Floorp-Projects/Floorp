/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.sync

import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.concept.sync.SyncManager
import mozilla.components.concept.sync.SyncStatusObserver
import mozilla.components.concept.sync.SyncableStore
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry
import java.io.Closeable
import java.lang.Exception
import java.util.concurrent.TimeUnit

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
    fun syncNow(startup: Boolean = false)
    fun startPeriodicSync(unit: TimeUnit, period: Long)
    fun stopPeriodicSync()
    fun workersStateChanged(isRunning: Boolean)
}

/**
 * A SyncManager implementation which uses WorkManager APIs to schedule sync tasks.
 */
class BackgroundSyncManager(
    private val syncScope: String
) : GeneralSyncManager() {
    override val logger = Logger("BgSyncManager")

    init {
        // Initialize the singleton observer. This must be called on the main thread.
        WorkersLiveDataObserver.init()
    }

    override fun createDispatcher(stores: Set<String>, account: OAuthAccount): SyncDispatcher {
        return WorkManagerSyncDispatcher(stores, syncScope, account)
    }

    override fun dispatcherUpdated(dispatcher: SyncDispatcher) {
        WorkersLiveDataObserver.setDispatcher(dispatcher)
    }
}

private val registry = ObserverRegistry<SyncStatusObserver>()

/**
 * A base SyncManager implementation which manages a dispatcher, handles authentication and requests
 * synchronization in the following manner:
 * On authentication AND on store set changes (add or remove)...
 * - immediate sync and periodic syncing are requested
 * On logout...
 * - periodic sync is requested to stop
 */
@SuppressWarnings("TooManyFunctions")
abstract class GeneralSyncManager
: SyncManager, Observable<SyncStatusObserver> by registry, SyncStatusObserver {
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
        const val SYNC_PERIOD = 4L
        val SYNC_PERIOD_UNIT = TimeUnit.HOURS
    }

    open val logger = Logger("GeneralSyncManager")

    private var account: OAuthAccount? = null
    private val stores = mutableSetOf<String>()

    // A SyncDispatcher instance bound to an account and a set of syncable stores.
    private var syncDispatcher: SyncDispatcher? = null

    override fun addStore(name: String) = synchronized(this) {
        logger.debug("Adding store: $name")
        stores.add(name)
        account?.let {
            syncDispatcher = newDispatcher(syncDispatcher, stores, it)
            initDispatcher(syncDispatcher!!)
        }
        Unit
    }

    override fun removeStore(name: String) = synchronized(this) {
        logger.debug("Removing store: $name")
        stores.remove(name)
        account?.let {
            syncDispatcher = newDispatcher(syncDispatcher, stores, it)
            initDispatcher(syncDispatcher!!)
        }
        Unit
    }

    override fun authenticated(account: OAuthAccount) = synchronized(this) {
        logger.debug("Authenticated")
        this.account = account
        syncDispatcher = newDispatcher(syncDispatcher, stores, account)
        initDispatcher(syncDispatcher!!)
        logger.debug("set and initialized new dispatcher: $syncDispatcher")
    }

    override fun loggedOut() = synchronized(this) {
        logger.debug("Logging out")
        account = null
        syncDispatcher!!.stopPeriodicSync()
        syncDispatcher!!.close()
        syncDispatcher = null
    }

    override fun syncNow(startup: Boolean) = synchronized(this) {
        logger.debug("Requesting immediate sync")
        syncDispatcher!!.syncNow(startup)
    }

    override fun isSyncRunning(): Boolean {
        syncDispatcher?.let { return it.isSyncActive() }
        return false
    }

    abstract fun createDispatcher(stores: Set<String>, account: OAuthAccount): SyncDispatcher

    abstract fun dispatcherUpdated(dispatcher: SyncDispatcher)

    // Manager encapsulates events emitted by the wrapped observers, and passes them on to the outside world.
    // This split allows us to define a nice public observer API (manager -> consumers), along with
    // a more robust internal observer API (dispatcher -> manager). Currently the interfaces are the same.

    override fun onStarted() {
        notifyObservers { onStarted() }
    }

    override fun onIdle() {
        notifyObservers { onIdle() }
    }

    override fun onError(error: Exception?) {
        notifyObservers { onError(error) }
    }

    private fun newDispatcher(
        currentDispatcher: SyncDispatcher?,
        stores: Set<String>,
        account: OAuthAccount
    ): SyncDispatcher {
        // Let the existing dispatcher, if present, cleanup.
        currentDispatcher?.close()
        // TODO will events from old and new dispatchers overlap..? How do we ensure correct sequence
        // for outside observers?
        currentDispatcher?.unregister(this)

        // Create a new dispatcher bound to current stores and account.
        return createDispatcher(stores, account)
    }

    private fun initDispatcher(dispatcher: SyncDispatcher) {
        dispatcher.register(this@GeneralSyncManager)
        dispatcher.syncNow()
        dispatcher.startPeriodicSync(SYNC_PERIOD_UNIT, SYNC_PERIOD)
        dispatcherUpdated(dispatcher)
    }
}
