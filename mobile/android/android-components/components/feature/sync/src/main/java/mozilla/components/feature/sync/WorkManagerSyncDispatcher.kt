/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.sync

import android.content.Context
import android.support.annotation.UiThread
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
import mozilla.components.concept.sync.OAuthAccount
import mozilla.components.concept.sync.SyncStatus
import mozilla.components.concept.sync.SyncStatusObserver
import mozilla.components.service.fxa.SharedPrefAccountStorage
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.base.observer.Observable
import mozilla.components.support.base.observer.ObserverRegistry
import java.io.Closeable
import java.util.concurrent.TimeUnit

private enum class SyncWorkerTag {
    Common,
    Immediate,
    Periodic
}

private enum class SyncWorkerName {
    Periodic,
    Immediate
}

/**
 * A singleton wrapper around the the LiveData "forever" observer - i.e. an observer not bound
 * to a lifecycle owner. This observer is always active.
 * We will have different dispatcher instances throughout the lifetime of the app, but always a
 * single LiveData instance.
 */
object WorkersLiveDataObserver {
    private val workersLiveData = WorkManager.getInstance().getWorkInfosByTagLiveData(
        SyncWorkerTag.Common.name
    )

    private var dispatcher: SyncDispatcher? = null

    @UiThread
    fun init() {
        workersLiveData.observeForever {
            val isRunningOrNothing = it?.any { worker -> worker.state == WorkInfo.State.RUNNING }
            val isRunning = when (isRunningOrNothing) {
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

class WorkManagerSyncDispatcher(
    private val stores: Set<String>,
    private val syncScope: String,
    private val account: OAuthAccount
) : SyncDispatcher, Observable<SyncStatusObserver> by ObserverRegistry(), Closeable {
    private val logger = Logger("WMSyncDispatcher")

    // TODO does this need to be volatile?
    private var isSyncActive = false

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

    override fun syncNow(startup: Boolean) {
        logger.debug("Immediate sync requested, startup = $startup")
        val delayMs = if (startup) {
            // Startup delay is there to avoid SQLITE_BUSY crashes, since we currently do a poor job
            // of managing database connections, and we expect there to be database writes at the start.
            // FIXME https://github.com/mozilla-mobile/android-components/issues/1369
            SYNC_STARTUP_DELAY_MS
        } else {
            0L
        }
        WorkManager.getInstance().beginUniqueWork(
            SyncWorkerName.Immediate.name,
            // Use the 'keep' policy to minimize overhead from multiple "sync now" operations coming in
            // at the same time.
            ExistingWorkPolicy.KEEP,
            immediateSyncWorkRequest(delayMs)
        ).enqueue()
    }

    override fun close() {
        stopPeriodicSync()
    }

    /**
     * Periodic background syncing is mainly intended to reduce workload when we sync during
     * application startup.
     */
    override fun startPeriodicSync(unit: TimeUnit, period: Long) {
        logger.debug("Starting periodic syncing, period = $period, time unit = $unit")
        // Use the 'replace' policy as a simple way to upgrade periodic worker configurations across
        // application versions. We do this instead of versioning workers.
        WorkManager.getInstance().enqueueUniquePeriodicWork(
            SyncWorkerName.Periodic.name,
            ExistingPeriodicWorkPolicy.REPLACE,
            periodicSyncWorkRequest(unit, period)
        )
    }

    override fun stopPeriodicSync() {
        logger.debug("Cancelling periodic syncing")
        WorkManager.getInstance().cancelUniqueWork(SyncWorkerName.Periodic.name)
    }

    private fun periodicSyncWorkRequest(unit: TimeUnit, period: Long): PeriodicWorkRequest {
        val data = getWorkerData()
        // Periodic interval must be at least PeriodicWorkRequest.MIN_PERIODIC_INTERVAL_MILLIS,
        // e.g. not more frequently than 15 minutes.
        return PeriodicWorkRequestBuilder<WorkManagerSyncWorker>(period, unit)
                .setInputData(data)
                .addTag(SyncWorkerTag.Common.name)
                .addTag(SyncWorkerTag.Periodic.name)
                .build()
    }

    private fun immediateSyncWorkRequest(delayMs: Long = 0L): OneTimeWorkRequest {
        val data = getWorkerData()
        return OneTimeWorkRequestBuilder<WorkManagerSyncWorker>()
                .setConstraints(
                        Constraints.Builder()
                                .setRequiredNetworkType(NetworkType.CONNECTED)
                                .build()
                )
                .setInputData(data)
                .addTag(SyncWorkerTag.Common.name)
                .addTag(SyncWorkerTag.Immediate.name)
                .setInitialDelay(delayMs, TimeUnit.MILLISECONDS)
                .build()
    }

    private fun getWorkerData(): Data {
        val dataBuilder = Data.Builder()
                .putString("account", account.toJSONString())
                .putString("syncScope", syncScope)
                .putStringArray("stores", stores.toTypedArray())

        return dataBuilder.build()
    }
}

class WorkManagerSyncWorker(
    private val context: Context,
    private val params: WorkerParameters
) : CoroutineWorker(context, params) {
    private val logger = Logger("SyncWorker")

    private fun isPeriodic(): Boolean {
        return params.tags.contains(SyncWorkerTag.Periodic.name)
    }

    private fun lastSyncedWithinStaggerBuffer(): Boolean {
        val lastSyncedTs = getLastSynced(context)
        return lastSyncedTs != 0L && (System.currentTimeMillis() - lastSyncedTs) < SYNC_STAGGER_BUFFER_MS
    }

    @Suppress("ReturnCount", "ComplexMethod")
    override suspend fun doWork(): Result {
        logger.debug("Starting sync... Tagged as: ${params.tags}")

        // If this is a periodic sync task, and we've very recently synced successfully, skip it.
        // There could have been a manual or heuristic-driven syc very recently, and it's unlikely
        // that an additional sync so shortly afterward will be valuable.
        // NB: this does not affect manually triggered syncing in any way.
        if (isPeriodic() && lastSyncedWithinStaggerBuffer()) {
            return Result.success()
        }

        // Otherwise, proceed as normal.
        // Read all of the data we'll need to sync: account, syncScope and a map of SyncableStore objects.
        val accountStorage = SharedPrefAccountStorage(context)
        val account = accountStorage.read()
                // Account must have disappeared in the time between this task being scheduled and executed.
                // A "logout" is expected to cancel pending sync tasks, but that isn't guaranteed.
                ?: return Result.failure()
        val syncScope = params.inputData.getString("syncScope")!!
        val syncableStores = params.inputData.getStringArray("stores")!!.associate {
            it to GlobalSyncableStoreProvider.getStore(it)!!
        }

        val syncResult = StorageSync(syncableStores, syncScope).sync(account)

        val resultBuilder = Data.Builder()
        syncResult.forEach {
            val status = it.value.status
            when (status) {
                SyncStatus.Ok -> {
                    logger.info("Synchronized store ${it.key}")
                    resultBuilder.putBoolean(it.key, true)
                }
                is SyncStatus.Error -> {
                    logger.error("Failed to synchronize store ${it.key}", status.exception)
                    resultBuilder.putBoolean(it.key, false)
                }
            }
        }

        // Worker should set the "last-synced" timestamp, and since we have a single timestamp,
        // it's not clear if a single failure should prevent its update. That's the current behaviour
        // in Fennec, but for very specific reasons that aren't relevant here. We could have
        // a timestamp per store, or whatever we want here really.
        // For now, we just update it every time we attempt to sync, regardless of the results.
        setLastSynced(context, System.currentTimeMillis())

        // Always return 'success' here. In the future, we could inspect exceptions in SyncError
        // and request a 'retry' if we see temporary problems such as network errors.
        return Result.success(resultBuilder.build())
    }
}

private const val SYNC_STATE_PREFS_KEY = "syncPrefs"
private const val SYNC_LAST_SYNCED_KEY = "lastSynced"

private const val SYNC_STAGGER_BUFFER_MS = 10 * 60 * 1000L // 10 minutes.
private const val SYNC_STARTUP_DELAY_MS = 5 * 1000L // 5 seconds.

fun getLastSynced(context: Context): Long {
    return context
        .getSharedPreferences(SYNC_STATE_PREFS_KEY, Context.MODE_PRIVATE)
        .getLong(SYNC_LAST_SYNCED_KEY, 0)
}

fun setLastSynced(context: Context, ts: Long) {
    context
        .getSharedPreferences(SYNC_STATE_PREFS_KEY, Context.MODE_PRIVATE)
        .edit()
        .putLong(SYNC_LAST_SYNCED_KEY, ts)
        .apply()
}
