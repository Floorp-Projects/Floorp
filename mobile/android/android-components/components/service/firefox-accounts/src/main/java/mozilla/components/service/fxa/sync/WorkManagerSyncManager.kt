/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.fxa.sync

import android.content.Context
import androidx.annotation.UiThread
import androidx.annotation.VisibleForTesting
import androidx.work.BackoffPolicy
import androidx.work.Constraints
import androidx.work.CoroutineWorker
import androidx.work.Data
import androidx.work.ExistingPeriodicWorkPolicy
import androidx.work.ExistingWorkPolicy
import androidx.work.NetworkType
import androidx.work.OneTimeWorkRequest
import androidx.work.OneTimeWorkRequestBuilder
import androidx.work.PeriodicWorkRequest
import androidx.work.PeriodicWorkRequestBuilder
import androidx.work.WorkInfo
import androidx.work.WorkManager
import androidx.work.WorkerParameters
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import mozilla.appservices.sync15.SyncTelemetryPing
import mozilla.appservices.syncmanager.ServiceStatus
import mozilla.appservices.syncmanager.SyncEngineSelection
import mozilla.appservices.syncmanager.SyncParams
import mozilla.components.concept.storage.KeyProvider
import mozilla.components.service.fxa.FxaDeviceSettingsCache
import mozilla.components.service.fxa.SyncAuthInfoCache
import mozilla.components.service.fxa.SyncConfig
import mozilla.components.service.fxa.SyncEngine
import mozilla.components.service.fxa.manager.GlobalAccountManager
import mozilla.components.service.fxa.manager.SyncEnginesStorage
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry
import mozilla.components.support.sync.telemetry.SyncTelemetry
import java.io.Closeable
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.TimeUnit
import mozilla.appservices.syncmanager.SyncManager as RustSyncManager

@VisibleForTesting
internal enum class SyncWorkerTag {
    Common,
    Immediate, // will not debounce a sync
    Debounce, // will debounce if another sync happened recently
}

private enum class SyncWorkerName {
    Periodic,
    Immediate,
}

private const val KEY_DATA_STORES = "stores"
private const val KEY_REASON = "reason"

private const val SYNC_WORKER_BACKOFF_DELAY_MINUTES = 3L

/**
 * A [SyncManager] implementation which uses WorkManager APIs to schedule sync tasks.
 *
 * Must be initialized on the main thread.
 */
internal class WorkManagerSyncManager(
    private val context: Context,
    syncConfig: SyncConfig,
) : SyncManager(syncConfig) {
    override val logger = Logger("BgSyncManager")

    init {
        WorkersLiveDataObserver.init(context)

        if (syncConfig.periodicSyncConfig == null) {
            logger.info("Periodic syncing is disabled.")
        } else {
            logger.info("Periodic syncing enabled: ${syncConfig.periodicSyncConfig}")
        }
    }

    override fun createDispatcher(supportedEngines: Set<SyncEngine>): SyncDispatcher {
        return WorkManagerSyncDispatcher(context, supportedEngines)
    }

    override fun dispatcherUpdated(dispatcher: SyncDispatcher) {
        WorkersLiveDataObserver.setDispatcher(dispatcher)
    }
}

/**
 * A singleton wrapper around the the LiveData "forever" observer - i.e. an observer not bound
 * to a lifecycle owner. This observer is always active.
 * We will have different dispatcher instances throughout the lifetime of the app, but always a
 * single LiveData instance.
 */
internal object WorkersLiveDataObserver {
    private lateinit var workManager: WorkManager
    private val workersLiveData by lazy {
        workManager.getWorkInfosByTagLiveData(SyncWorkerTag.Common.name)
    }

    private var dispatcher: SyncDispatcher? = null

    /**
     * Initializes the Observer.
     *
     * @param context the context that will be used to with the [WorkManager] to observe workers.
     */
    @UiThread
    fun init(context: Context) {
        workManager = WorkManager.getInstance(context)

        // Only set our observer once.
        if (workersLiveData.hasObservers()) return

        // This must be called on the UI thread.
        workersLiveData.observeForever {
            val isRunning = when (it?.any { worker -> worker.state == WorkInfo.State.RUNNING }) {
                null -> false
                false -> false
                true -> true
            }

            dispatcher?.workersStateChanged(isRunning)

            // TODO process errors coming out of worker.outputData
        }
    }

    fun setDispatcher(dispatcher: SyncDispatcher) {
        this.dispatcher = dispatcher
    }
}

internal class WorkManagerSyncDispatcher(
    private val context: Context,
    private val supportedEngines: Set<SyncEngine>,
) : SyncDispatcher, Observable<SyncStatusObserver> by ObserverRegistry(), Closeable {
    private val logger = Logger("WMSyncDispatcher")

    // TODO does this need to be volatile?
    private var isSyncActive = false

    init {
        // Stop any currently active periodic syncing. Consumers of this class are responsible for
        // starting periodic syncing via [startPeriodicSync] if they need it.
        stopPeriodicSync()
    }

    override fun workersStateChanged(isRunning: Boolean) {
        if (isSyncActive && !isRunning) {
            notifyObservers { onIdle() }
            isSyncActive = false
        } else if (!isSyncActive && isRunning) {
            notifyObservers { onStarted() }
            isSyncActive = true
        }
    }

    override fun isSyncActive(): Boolean {
        return isSyncActive
    }

    override fun syncNow(
        reason: SyncReason,
        debounce: Boolean,
        customEngineSubset: List<SyncEngine>,
    ) {
        logger.debug("Immediate sync requested, reason = $reason, debounce = $debounce")
        val delayMs = if (reason == SyncReason.Startup) {
            // Startup delay is there to avoid SQLITE_BUSY crashes, since we currently do a poor job
            // of managing database connections, and we expect there to be database writes at the start.
            // We've done bunch of work to make this better (see https://github.com/mozilla-mobile/android-components/issues/1369),
            // but it's not clear yet this delay is completely safe to remove.
            SYNC_STARTUP_DELAY_MS
        } else {
            0L
        }
        WorkManager.getInstance(context).beginUniqueWork(
            SyncWorkerName.Immediate.name,
            // Use the 'keep' policy to minimize overhead from multiple "sync now" operations coming in
            // at the same time.
            ExistingWorkPolicy.KEEP,
            regularSyncWorkRequest(reason, delayMs, debounce, customEngineSubset),
        ).enqueue()
    }

    override fun close() {
        unregisterObservers()
        stopPeriodicSync()
    }

    /**
     * Periodic background syncing is mainly intended to reduce workload when we sync during
     * application startup.
     */
    override fun startPeriodicSync(unit: TimeUnit, period: Long, initialDelay: Long) {
        logger.debug("Starting periodic syncing, period = $period, time unit = $unit")
        // Use the 'replace' policy as a simple way to upgrade periodic worker configurations across
        // application versions. We do this instead of versioning workers.
        WorkManager.getInstance(context).enqueueUniquePeriodicWork(
            SyncWorkerName.Periodic.name,
            ExistingPeriodicWorkPolicy.REPLACE,
            periodicSyncWorkRequest(unit, period, initialDelay),
        )
    }

    /**
     * Disables periodic syncing in the background. Currently running syncs may continue until completion.
     * Safe to call this even if periodic syncing isn't currently enabled.
     */
    override fun stopPeriodicSync() {
        logger.debug("Cancelling periodic syncing")
        WorkManager.getInstance(context).cancelUniqueWork(SyncWorkerName.Periodic.name)
    }

    private fun periodicSyncWorkRequest(unit: TimeUnit, period: Long, initialDelay: Long): PeriodicWorkRequest {
        val data = getWorkerData(SyncReason.Scheduled)
        // Periodic interval must be at least PeriodicWorkRequest.MIN_PERIODIC_INTERVAL_MILLIS,
        // e.g. not more frequently than 15 minutes.
        return PeriodicWorkRequestBuilder<WorkManagerSyncWorker>(period, unit, initialDelay, unit)
            .setConstraints(
                Constraints.Builder()
                    .setRequiredNetworkType(NetworkType.CONNECTED)
                    .build(),
            )
            .setInputData(data)
            .addTag(SyncWorkerTag.Common.name)
            .addTag(SyncWorkerTag.Debounce.name)
            .setBackoffCriteria(
                BackoffPolicy.EXPONENTIAL,
                SYNC_WORKER_BACKOFF_DELAY_MINUTES,
                TimeUnit.MINUTES,
            )
            .build()
    }

    private fun regularSyncWorkRequest(
        reason: SyncReason,
        delayMs: Long = 0L,
        debounce: Boolean = false,
        customEngineSubset: List<SyncEngine> = listOf(),
    ): OneTimeWorkRequest {
        val data = getWorkerData(reason, customEngineSubset)
        return OneTimeWorkRequestBuilder<WorkManagerSyncWorker>()
            .setConstraints(
                Constraints.Builder()
                    .setRequiredNetworkType(NetworkType.CONNECTED)
                    .build(),
            )
            .setInputData(data)
            .addTag(SyncWorkerTag.Common.name)
            .addTag(if (debounce) SyncWorkerTag.Debounce.name else SyncWorkerTag.Immediate.name)
            .setInitialDelay(delayMs, TimeUnit.MILLISECONDS)
            .setBackoffCriteria(
                BackoffPolicy.EXPONENTIAL,
                SYNC_WORKER_BACKOFF_DELAY_MINUTES,
                TimeUnit.MINUTES,
            )
            .build()
    }

    @VisibleForTesting
    internal fun getWorkerData(
        reason: SyncReason,
        customEngineSubset: List<SyncEngine> = listOf(),
    ): Data {
        val enginesToSync = customEngineSubset.takeIf { it.isNotEmpty() } ?: supportedEngines
        return Data.Builder()
            .putStringArray(KEY_DATA_STORES, enginesToSync.map { it.nativeName }.toTypedArray())
            .putString(KEY_REASON, reason.asString())
            .build()
    }
}

internal class WorkManagerSyncWorker(
    private val context: Context,
    private val params: WorkerParameters,
) : CoroutineWorker(context, params) {
    private val logger = Logger("SyncWorker")

    @VisibleForTesting
    internal fun isDebounced(): Boolean {
        return params.tags.contains(SyncWorkerTag.Debounce.name)
    }

    @VisibleForTesting
    internal fun lastSyncedWithinStaggerBuffer(engine: String): Boolean {
        if (!isDebounced()) {
            return false
        }

        return engineSyncTimestamp[engine]?.let {
            (System.currentTimeMillis() - it) < SYNC_STAGGER_BUFFER_MS
        } ?: false
    }

    private fun updateEngineSyncedTime(engine: String) {
        engineSyncTimestamp[engine] = System.currentTimeMillis()
    }

    override suspend fun doWork(): Result = withContext(Dispatchers.IO) {
        logger.debug("Starting sync... Tagged as: ${params.tags}")

        // We will need a list of SyncableStores.
        val syncableStores = params.inputData.getStringArray(KEY_DATA_STORES)?.filter {
            !lastSyncedWithinStaggerBuffer(it)
        }?.associate {
            // Convert from a string back to our SyncEngine type.
            val engine = when (it) {
                SyncEngine.History.nativeName -> SyncEngine.History
                SyncEngine.Bookmarks.nativeName -> SyncEngine.Bookmarks
                SyncEngine.Passwords.nativeName -> SyncEngine.Passwords
                SyncEngine.Tabs.nativeName -> SyncEngine.Tabs
                SyncEngine.CreditCards.nativeName -> SyncEngine.CreditCards
                SyncEngine.Addresses.nativeName -> SyncEngine.Addresses
                else -> throw IllegalStateException("Invalid syncable store: $it")
            }

            updateEngineSyncedTime(engine.nativeName)
            engine to checkNotNull(GlobalSyncableStoreProvider.getLazyStoreWithKey(engine)) {
                "SyncableStore missing from GlobalSyncableStoreProvider: ${engine.nativeName}"
            }
        }

        if (syncableStores.isNullOrEmpty()) {
            // Short-circuit if there are no configured stores.
            // Don't update the "last-synced" timestamp because we haven't actually synced anything.
            Result.success()
        } else {
            doSync(syncableStores)
        }
    }

    @Suppress("LongMethod", "ComplexMethod")
    private suspend fun doSync(syncableStores: Map<SyncEngine, LazyStoreWithKey>): Result {
        val syncManager = RustSyncManager()
        val engineKeyProviders = mutableMapOf<SyncEngine, KeyProvider>()

        // We need to tell RustSyncManager which engines to sync.
        val enginesToSync = SyncEngineSelection.Some(syncableStores.map { it.key.nativeName })

        // We need to tell RustSyncManager about instances of supported stores ('places' and 'logins').
        syncableStores.entries.forEach {
            // We're assuming all syncable stores live in Rust.
            // Currently `RustSyncManager` doesn't support non-Rust sync engines.
            when (it.key) {
                SyncEngine.History -> it.value.lazyStore.value.registerWithSyncManager()
                SyncEngine.Bookmarks -> it.value.lazyStore.value.registerWithSyncManager()
                SyncEngine.CreditCards -> {
                    it.value.lazyStore.value.registerWithSyncManager()

                    checkNotNull(it.value.keyProvider) {
                        "CreditCards store must be configured with a KeyProvider"
                    }

                    engineKeyProviders[it.key] = it.value.keyProvider!!.value
                }
                SyncEngine.Addresses -> {
                    it.value.lazyStore.value.registerWithSyncManager()
                }
                SyncEngine.Passwords -> {
                    it.value.lazyStore.value.registerWithSyncManager()

                    checkNotNull(it.value.keyProvider) {
                        "Passwords store must be configured with a KeyProvider"
                    }

                    engineKeyProviders[it.key] = it.value.keyProvider!!.value
                }
                SyncEngine.Tabs -> {
                    it.value.lazyStore.value.registerWithSyncManager()
                }
                else -> throw NotImplementedError("Unsupported engine: ${it.key}")
            }
        }

        // We need to know the reason we're syncing.
        val reason = params.inputData.getString(KEY_REASON)!!.toSyncReason()

        // We need a cached "sync auth info" object.
        val syncAuthInfo = SyncAuthInfoCache(context).getCached() ?: return Result.failure()

        // We need any persisted state that we received from RustSyncManager in the past.
        // We should be able to pass a `null` value, but currently the library will crash.
        // See https://github.com/mozilla/application-services/issues/1829
        val currentSyncState = getSyncState(context) ?: ""

        // We need to tell RustSyncManager about our local "which engines are enabled/disabled" state.
        // This is a global state, shared by every sync client for this account. State that we will
        // pass here will overwrite current global state and may be propagated to every sync client.
        // This should be empty if there have been no changes to this state.
        // We pass this state if user changed it (EngineChange) or if we're in a first sync situation.
        // A "first sync" will happen after signing up or signing in.
        // In both cases, user may have been asked which engines they'd like to sync.
        val enabledChanges = when (reason) {
            SyncReason.EngineChange, SyncReason.FirstSync -> {
                val engineMap = SyncEnginesStorage(context).getStatus().toMutableMap()
                // For historical reasons, a "history engine" really means two sync collections: history and forms.
                // Underlying library doesn't manage this for us, and other clients will get confused
                // if we modify just "history" without also modifying "forms", and vice versa.
                // So: whenever a change to the "history" engine happens locally, inject the same "forms" change.
                // This should be handled by RustSyncManager. See https://github.com/mozilla/application-services/issues/1832
                engineMap[SyncEngine.History]?.let {
                    engineMap[SyncEngine.Forms] = it
                }
                engineMap.mapKeys { it.key.nativeName }
            }
            else -> emptyMap()
        }

        // We need to tell RustSyncManager about our current FxA device. It needs that information
        // in order to sync the 'clients' collection.
        // We're depending on cache being populated. An alternative to using a "cache" is to
        // configure the worker with the values stored in it: device name, type and fxaDeviceID.
        // While device type and fxaDeviceID are stable, users are free to change device name whenever.
        // We need to reflect these changes during a sync. Our options are then: provide a global cache,
        // or re-configure our workers every time a change is made to the device name.
        // We have the same basic story already with syncAuthInfo cache, and a cache is much easier
        // to implement/reason about than worker reconfiguration.
        val deviceSettings = FxaDeviceSettingsCache(context).getCached()!!

        // Obtain encryption keys for stores that came along with KeyProviders.
        // This can take a bit of time!
        val localEncryptionKeys = engineKeyProviders.mapKeys {
            it.key.nativeName
        }.mapValues {
            it.value.getOrGenerateKey().key
        }

        // We're now ready to sync.
        val syncParams = SyncParams(
            reason = reason.toRustSyncReason(),
            engines = enginesToSync,
            authInfo = syncAuthInfo.toNative(),
            enabledChanges = enabledChanges,
            persistedState = currentSyncState,
            deviceSettings = deviceSettings,
            localEncryptionKeys = localEncryptionKeys,
        )

        val syncResult = syncManager.sync(syncParams)

        // Persist the sync state; it may have changed during a sync, and RustSyncManager relies on us
        // to store it.
        setSyncState(context, syncResult.persistedState)

        // Log the results.
        syncResult.failures.entries.forEach {
            logger.error("Failed to sync ${it.key}, reason: ${it.value}")
        }
        syncResult.successful.forEach {
            logger.info("Successfully synced $it")
        }

        // Process any changes to the list of declined/accepted engines.
        // NB: We may have received engine state information about an engine we're unfamiliar with.
        // `SyncEngine.Other(string)` stands in for unknown engines.
        val declinedEngines = syncResult.declined?.map { it.toSyncEngine() }?.toSet() ?: emptySet()
        // We synthesize the 'accepted' list ourselves: engines we know about - declined engines.
        // This assumes that "engines we know about" is a subset of engines RustSyncManager knows about.
        // RustSyncManager will handle this, eventually.
        // See: https://github.com/mozilla/application-services/issues/1685
        val acceptedEngines = syncableStores.keys.filter { !declinedEngines.contains(it) }

        // Persist engine state changes.
        with(SyncEnginesStorage(context)) {
            declinedEngines.forEach { setStatus(it, status = false) }
            acceptedEngines.forEach { setStatus(it, status = true) }
        }

        // Process telemetry.
        syncResult.telemetryJson?.let { SyncTelemetry.processSyncTelemetry(SyncTelemetryPing.fromJSONString(it)) }

        // Finally, declare success, failure or request a retry based on 'sync status'.
        return when (syncResult.status) {
            // Happy case.
            ServiceStatus.OK -> {
                // Worker should set the "last-synced" timestamp, and since we have a single timestamp,
                // it's not clear if a single failure should prevent its update. That's the current behaviour
                // in Fennec, but for very specific reasons that aren't relevant here. We could have
                // a timestamp per store, or whatever we want here really.
                // For now, we just update it every time we succeed to sync.
                setLastSynced(context, System.currentTimeMillis())
                Result.success()
            }

            // Retry cases.
            // NB: retry doesn't mean "immediate retry". It means "retry, but respecting this worker's
            // backoff policy, as configured during worker's creation.
            // TODO FOR ALL retries: look at workerParams.mRunAttemptCount, don't retry after a certain number.
            ServiceStatus.NETWORK_ERROR -> {
                logger.error("Network error")
                Result.retry()
            }
            ServiceStatus.BACKED_OFF -> {
                logger.error("Backed-off error")
                // As part of `syncResult`, we get back `nextSyncAllowedAt`. Ideally, we should not retry
                // before that passes. However, we can not reconfigure back-off policy for an already
                // created Worker. So, we just rely on a sensible default. `RustSyncManager` will fail
                // to sync with a BACKED_OFF error without hitting the server if we don't respect
                // `nextSyncAllowedAt`, so we should be good either way.
                Result.retry()
            }

            // Failure cases.
            ServiceStatus.AUTH_ERROR -> {
                logger.error("Auth error")
                GlobalAccountManager.authError("RustSyncManager.sync")
                Result.failure()
            }
            ServiceStatus.SERVICE_ERROR -> {
                logger.error("Service error")
                Result.failure()
            }
            ServiceStatus.OTHER_ERROR -> {
                logger.error("'Other' error :(")
                Result.failure()
            }
        }
    }

    companion object {
        @VisibleForTesting
        internal const val SYNC_STAGGER_BUFFER_MS = 5 * 1000L // 5 seconds.

        @VisibleForTesting
        internal val engineSyncTimestamp = ConcurrentHashMap<String, Long>()
    }
}

private const val SYNC_STATE_PREFS_KEY = "syncPrefs"
private const val SYNC_LAST_SYNCED_KEY = "lastSynced"
private const val SYNC_STATE_KEY = "persistedState"

private const val SYNC_STARTUP_DELAY_MS = 5 * 1000L // 5 seconds.

fun getLastSynced(context: Context): Long {
    return context
        .getSharedPreferences(SYNC_STATE_PREFS_KEY, Context.MODE_PRIVATE)
        .getLong(SYNC_LAST_SYNCED_KEY, 0)
}

internal fun clearSyncState(context: Context) {
    context.getSharedPreferences(SYNC_STATE_PREFS_KEY, Context.MODE_PRIVATE)
        .edit().clear().apply()
}

internal fun getSyncState(context: Context): String? {
    return context
        .getSharedPreferences(SYNC_STATE_PREFS_KEY, Context.MODE_PRIVATE)
        .getString(SYNC_STATE_KEY, null)
}

internal fun setLastSynced(context: Context, ts: Long) {
    context
        .getSharedPreferences(SYNC_STATE_PREFS_KEY, Context.MODE_PRIVATE)
        .edit()
        .putLong(SYNC_LAST_SYNCED_KEY, ts)
        .apply()
}

internal fun setSyncState(context: Context, state: String) {
    context
        .getSharedPreferences(SYNC_STATE_PREFS_KEY, Context.MODE_PRIVATE)
        .edit()
        .putString(SYNC_STATE_KEY, state)
        .apply()
}
