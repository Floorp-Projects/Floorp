/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.syncedtabs.commands

import android.content.Context
import androidx.work.BackoffPolicy
import androidx.work.Constraints
import androidx.work.ExistingWorkPolicy
import androidx.work.NetworkType
import androidx.work.OneTimeWorkRequest
import androidx.work.OneTimeWorkRequestBuilder
import androidx.work.WorkManager
import androidx.work.WorkRequest
import java.util.concurrent.TimeUnit
import kotlin.time.Duration
import kotlin.time.Duration.Companion.milliseconds

/**
 * Schedules a worker to flush any pending [SyncedTabsCommands].
 *
 * @property context The Android application context.
 * @property flushDelay The minimum delay between scheduling and triggering a flush.
 * @property retryDelay The minimum time to wait before retrying a flush.
 */
class SyncedTabsCommandsFlushScheduler(
    private val context: Context,
    private val flushDelay: Duration = Duration.ZERO,
    private val retryDelay: Duration = WorkRequest.MIN_BACKOFF_MILLIS.milliseconds,
) {
    /** Schedules a flush. */
    fun requestFlush() {
        WorkManager.getInstance(context).enqueueUniqueWork(
            SyncedTabsCommandsFlushWorker.WORK_TAG,
            ExistingWorkPolicy.REPLACE,
            createFlushWorkRequest(),
        )
    }

    /** Cancels a previously-scheduled flush. */
    fun cancelFlush() {
        WorkManager.getInstance(context).cancelAllWorkByTag(SyncedTabsCommandsFlushWorker.WORK_TAG)
    }

    internal fun createFlushWorkRequest(): OneTimeWorkRequest {
        val constraints = Constraints.Builder().apply {
            setRequiredNetworkType(NetworkType.CONNECTED)
        }.build()

        return OneTimeWorkRequestBuilder<SyncedTabsCommandsFlushWorker>().apply {
            if (flushDelay > Duration.ZERO) {
                setInitialDelay(flushDelay.inWholeMilliseconds, TimeUnit.MILLISECONDS)
            }
            setConstraints(constraints)
            setBackoffCriteria(BackoffPolicy.EXPONENTIAL, retryDelay.inWholeMilliseconds, TimeUnit.MILLISECONDS)
            addTag(SyncedTabsCommandsFlushWorker.WORK_TAG)
        }.build()
    }
}
