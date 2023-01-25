/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import android.app.Activity
import android.app.Application
import android.content.Context
import kotlinx.coroutines.CoroutineDispatcher
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import mozilla.components.support.utils.ext.getPackageInfoCompat
import org.mozilla.fenix.android.DefaultActivityLifecycleCallbacks
import org.mozilla.fenix.ext.settings
import org.mozilla.fenix.nimbus.FxNimbus
import org.mozilla.fenix.utils.Settings
import java.text.SimpleDateFormat
import java.util.Calendar
import java.util.Locale

/**
 * Interface defining functions around persisted local state for certain metrics.
 */
interface MetricsStorage {
    /**
     * Determines whether an [event] should be sent based on locally-stored state.
     */
    suspend fun shouldTrack(event: Event): Boolean

    /**
     * Updates locally-stored state for an [event] that has just been sent.
     */
    suspend fun updateSentState(event: Event)

    /**
     * Will try to register this as a recorder of app usage based on whether usage recording is still
     * needed. It will measure usage by to monitoring lifecycle callbacks from [application]'s
     * activities and should update local state using [updateUsageState].
     */
    fun tryRegisterAsUsageRecorder(application: Application)

    /**
     * Update local state with a [usageLength] measurement.
     */
    fun updateUsageState(usageLength: Long)
}

internal class DefaultMetricsStorage(
    context: Context,
    private val settings: Settings,
    private val checkDefaultBrowser: () -> Boolean,
    private val shouldSendGenerally: () -> Boolean = { shouldSendGenerally(context) },
    private val getInstalledTime: () -> Long = { getInstalledTime(context) },
    private val dispatcher: CoroutineDispatcher = Dispatchers.IO,
) : MetricsStorage {

    private val dateFormatter = SimpleDateFormat("yyyy-MM-dd", Locale.US)

    /**
     * Checks local state to see whether the [event] should be sent.
     */
    override suspend fun shouldTrack(event: Event): Boolean =
        withContext(dispatcher) {
            // The side-effect of storing days of use always needs to happen.
            updateDaysOfUse()
            val currentTime = System.currentTimeMillis()
            shouldSendGenerally() && when (event) {
                Event.GrowthData.SetAsDefault -> {
                    currentTime.duringFirstMonth() &&
                        !settings.setAsDefaultGrowthSent &&
                        checkDefaultBrowser()
                }
                Event.GrowthData.FirstWeekSeriesActivity -> {
                    currentTime.duringFirstMonth() && shouldTrackFirstWeekActivity()
                }
                Event.GrowthData.SerpAdClicked -> {
                    currentTime.duringFirstMonth() && !settings.adClickGrowthSent
                }
                Event.GrowthData.UsageThreshold -> {
                    !settings.usageTimeGrowthSent &&
                        settings.usageTimeGrowthData > usageThresholdMillis
                }
            }
        }

    override suspend fun updateSentState(event: Event) = withContext(dispatcher) {
        when (event) {
            Event.GrowthData.SetAsDefault -> {
                settings.setAsDefaultGrowthSent = true
            }
            Event.GrowthData.FirstWeekSeriesActivity -> {
                settings.firstWeekSeriesGrowthSent = true
            }
            Event.GrowthData.SerpAdClicked -> {
                settings.adClickGrowthSent = true
            }
            Event.GrowthData.UsageThreshold -> {
                settings.usageTimeGrowthSent = true
            }
        }
    }

    override fun tryRegisterAsUsageRecorder(application: Application) {
        // Currently there is only interest in measuring usage during the first day of install.
        if (!settings.usageTimeGrowthSent && System.currentTimeMillis().duringFirstDay()) {
            application.registerActivityLifecycleCallbacks(UsageRecorder(this))
        }
    }

    override fun updateUsageState(usageLength: Long) {
        settings.usageTimeGrowthData += usageLength
    }

    private fun updateDaysOfUse() {
        val daysOfUse = settings.firstWeekDaysOfUseGrowthData
        val currentDate = Calendar.getInstance(Locale.US)
        val currentDateString = dateFormatter.format(currentDate.time)
        if (currentDate.timeInMillis.duringFirstWeek() && daysOfUse.none { it == currentDateString }) {
            settings.firstWeekDaysOfUseGrowthData = daysOfUse + currentDateString
        }
    }

    private fun shouldTrackFirstWeekActivity(): Boolean = Result.runCatching {
        if (!System.currentTimeMillis().duringFirstWeek() || settings.firstWeekSeriesGrowthSent) {
            return false
        }

        val daysOfUse = settings.firstWeekDaysOfUseGrowthData.map {
            dateFormatter.parse(it)
        }.sorted()

        // This loop will check whether the existing list of days of use, combined with the
        // current date, contains any periods of 3 days of use in a row.
        for (idx in daysOfUse.indices) {
            if (idx + 1 > daysOfUse.lastIndex || idx + 2 > daysOfUse.lastIndex) {
                continue
            }

            val referenceDate = daysOfUse[idx]!!.time.toCalendar()
            val secondDateEntry = daysOfUse[idx + 1]!!.time.toCalendar()
            val thirdDateEntry = daysOfUse[idx + 2]!!.time.toCalendar()
            val oneDayAfterReference = referenceDate.createNextDay()
            val twoDaysAfterReference = oneDayAfterReference.createNextDay()

            if (oneDayAfterReference == secondDateEntry && thirdDateEntry == twoDaysAfterReference) {
                return true
            }
        }
        return false
    }.getOrDefault(false)

    private fun Long.toCalendar(): Calendar = Calendar.getInstance(Locale.US).also { calendar ->
        calendar.timeInMillis = this
    }

    private fun Long.duringFirstDay() = this < getInstalledTime() + dayMillis

    private fun Long.duringFirstWeek() = this < getInstalledTime() + fullWeekMillis

    private fun Long.duringFirstMonth() = this < getInstalledTime() + shortestMonthMillis

    private fun Calendar.createNextDay() = (this.clone() as Calendar).also { calendar ->
        calendar.add(Calendar.DAY_OF_MONTH, 1)
    }

    /**
     * This will store app usage time to disk, based on Resume and Pause lifecycle events. Currently,
     * there is only interest in usage during the first day after install.
     */
    internal class UsageRecorder(
        private val metricsStorage: MetricsStorage,
    ) : DefaultActivityLifecycleCallbacks {
        private val activityStartTimes: MutableMap<String, Long?> = mutableMapOf()

        override fun onActivityResumed(activity: Activity) {
            super.onActivityResumed(activity)
            activityStartTimes[activity.componentName.toString()] = System.currentTimeMillis()
        }

        override fun onActivityPaused(activity: Activity) {
            super.onActivityPaused(activity)
            val startTime = activityStartTimes[activity.componentName.toString()] ?: return
            val elapsedTimeMillis = System.currentTimeMillis() - startTime
            metricsStorage.updateUsageState(elapsedTimeMillis)
        }
    }

    companion object {
        private const val dayMillis: Long = 1000 * 60 * 60 * 24
        private const val shortestMonthMillis: Long = dayMillis * 28

        // Note this is 8 so that recording of FirstWeekSeriesActivity happens throughout the length
        // of the 7th day after install
        private const val fullWeekMillis: Long = dayMillis * 8

        // The usage threshold we are interested in is currently 340 seconds.
        private const val usageThresholdMillis = 1000 * 340

        /**
         * Determines whether events should be tracked based on some general criteria:
         * - user has installed as a result of a campaign
         * - tracking is still enabled through Nimbus
         */
        fun shouldSendGenerally(context: Context): Boolean {
            return context.settings().adjustCampaignId.isNotEmpty() &&
                FxNimbus.features.growthData.value().enabled
        }

        fun getInstalledTime(context: Context): Long = context.packageManager
            .getPackageInfoCompat(context.packageName, 0)
            .firstInstallTime
    }
}
