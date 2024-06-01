/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.syncedtabs.commands

import android.content.Context
import androidx.work.CoroutineWorker
import androidx.work.WorkerParameters
import mozilla.components.concept.sync.DeviceCommandQueue

/**
 * A [CoroutineWorker] that flushes any pending [SyncedTabsCommands].
 *
 * @param context The Android application context.
 * @param params Parameters for this worker's internal state.
 */
class SyncedTabsCommandsFlushWorker(context: Context, params: WorkerParameters) :
    CoroutineWorker(context, params) {

    override suspend fun doWork(): Result {
        val commands = GlobalSyncedTabsCommandsProvider.requireCommands()
        return when (commands.flush()) {
            is DeviceCommandQueue.FlushResult.Ok -> Result.success()
            is DeviceCommandQueue.FlushResult.Retry -> Result.retry()
        }
    }

    internal companion object {
        const val WORK_TAG = "mozilla.components.feature.syncedtabs.commands.flush.work.tag"
    }
}
