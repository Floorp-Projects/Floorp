/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.perf

import android.app.usage.StorageStats
import android.app.usage.StorageStatsManager
import android.content.Context
import android.os.Build
import androidx.annotation.RequiresApi
import androidx.annotation.VisibleForTesting
import androidx.annotation.VisibleForTesting.PRIVATE
import androidx.annotation.WorkerThread
import androidx.core.content.getSystemService
import kotlinx.coroutines.DelicateCoroutinesApi
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import org.mozilla.fenix.GleanMetrics.StorageStats as Metrics

/**
 * A collection of functions related to measuring the [StorageStats] of the application such as data
 * dir size.
 *
 * Unfortunately, this API is only available on API 26+ so the data will only be reported for those
 * platforms.
 */
@RequiresApi(Build.VERSION_CODES.O) // StorageStatsManager
object StorageStatsMetrics {

    @OptIn(DelicateCoroutinesApi::class) // GlobalScope usage
    fun report(context: Context) {
        GlobalScope.launch(Dispatchers.IO) {
            reportSync(context)
        }
    }

    // I couldn't get runBlockingTest to work correctly so I moved the functionality under test to
    // a synchronous function.
    @VisibleForTesting(otherwise = PRIVATE)
    @WorkerThread // queryStatsForUid
    fun reportSync(context: Context) {
        // I don't expect this to ever be null so we don't report if so.
        context.getSystemService<StorageStatsManager>()?.let { storageStatsManager ->
            val appInfo = context.applicationInfo
            val storageStats = Metrics.queryStatsDuration.measure {
                // The docs say queryStatsForPackage may be slower if the app uses
                // android:sharedUserId so we the suggested alternative.
                //
                // The docs say this may be slow:
                // > This method may take several seconds to complete, so it should only be called
                // > from a worker thread.
                //
                // So we call from a worker thread and measure the duration to make sure it's not
                // too slow.
                storageStatsManager.queryStatsForUid(appInfo.storageUuid, appInfo.uid)
            }

            // dataBytes includes the cache so we subtract it.
            val justDataDirBytes = storageStats.dataBytes - storageStats.cacheBytes

            Metrics.dataDirBytes.accumulate(justDataDirBytes)
            Metrics.appBytes.accumulate(storageStats.appBytes)
            Metrics.cacheBytes.accumulate(storageStats.cacheBytes)
        }
    }
}
