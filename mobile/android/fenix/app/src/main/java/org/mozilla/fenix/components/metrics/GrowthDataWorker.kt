package org.mozilla.fenix.components.metrics

import android.content.Context
import androidx.work.ExistingWorkPolicy
import androidx.work.OneTimeWorkRequest
import androidx.work.WorkManager
import androidx.work.Worker
import androidx.work.WorkerParameters
import mozilla.components.support.utils.ext.getPackageInfoCompat
import org.mozilla.fenix.ext.metrics
import org.mozilla.fenix.ext.settings
import java.util.concurrent.TimeUnit

/**
 * Worker that will send the User Activated event at the end of the first week.
 */
class GrowthDataWorker(
    context: Context,
    workerParameters: WorkerParameters,
) : Worker(context, workerParameters) {

    override fun doWork(): Result {
        val settings = applicationContext.settings()

        if (!System.currentTimeMillis().isAfterFirstWeekFromInstall(applicationContext) ||
            settings.growthUserActivatedSent
        ) {
            return Result.success()
        }

        applicationContext.metrics.track(Event.GrowthData.UserActivated(fromSearch = false))

        return Result.success()
    }

    companion object {
        private const val GROWTH_USER_ACTIVATED_WORK_NAME = "org.mozilla.fenix.growth.work"
        private const val DAY_MILLIS: Long = 1000 * 60 * 60 * 24
        private const val FULL_WEEK_MILLIS: Long = DAY_MILLIS * 7

        /**
         * Schedules the Activated User event if needed.
         */
        fun sendActivatedSignalIfNeeded(context: Context) {
            val instanceWorkManager = WorkManager.getInstance(context)

            if (context.settings().growthUserActivatedSent) {
                return
            }

            val growthSignalWork = OneTimeWorkRequest.Builder(GrowthDataWorker::class.java)
                .setInitialDelay(FULL_WEEK_MILLIS, TimeUnit.MILLISECONDS)
                .build()

            instanceWorkManager.beginUniqueWork(
                GROWTH_USER_ACTIVATED_WORK_NAME,
                ExistingWorkPolicy.KEEP,
                growthSignalWork,
            ).enqueue()
        }

        /**
         * Returns [Boolean] value signaling if current time is after the first week after install.
         */
        private fun Long.isAfterFirstWeekFromInstall(context: Context): Boolean {
            val timeDifference = this - getInstalledTime(context)
            return (FULL_WEEK_MILLIS <= timeDifference)
        }

        private fun getInstalledTime(context: Context): Long = context.packageManager
            .getPackageInfoCompat(context.packageName, 0)
            .firstInstallTime
    }
}
