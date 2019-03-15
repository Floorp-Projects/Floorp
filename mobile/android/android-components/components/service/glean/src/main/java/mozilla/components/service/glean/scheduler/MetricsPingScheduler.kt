/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.scheduler

import android.content.Context
import android.content.SharedPreferences
import android.support.annotation.VisibleForTesting
import android.text.format.DateUtils
import androidx.work.ExistingWorkPolicy
import androidx.work.OneTimeWorkRequestBuilder
import androidx.work.Worker
import androidx.work.WorkManager
import androidx.work.WorkerParameters
import kotlinx.coroutines.launch
import mozilla.components.service.glean.Dispatchers
import mozilla.components.service.glean.Glean
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.service.glean.utils.getISOTimeString
import mozilla.components.service.glean.utils.parseISOTimeString
import mozilla.components.service.glean.TimeUnit
import java.util.Calendar
import java.util.Date
import java.util.concurrent.TimeUnit as AndroidTimeUnit

/**
 * MetricsPingScheduler facilitates scheduling the periodic assembling of metrics pings,
 * at a given time, trying its best to handle the following cases:
 *
 * - ping is overdue (due time already passed) for the current calendar day;
 * - ping is soon to be sent in the current calendar day;
 * - ping was already sent, and must be scheduled for the next calendar day.
 */
internal class MetricsPingScheduler(val applicationContext: Context) {
    private val logger = Logger("glean/MetricsPingScheduler")
    internal val sharedPreferences: SharedPreferences by lazy {
        applicationContext.getSharedPreferences(this.javaClass.canonicalName, Context.MODE_PRIVATE)
    }

    companion object {
        const val LAST_METRICS_PING_SENT_DATETIME = "last_metrics_ping_iso_datetime"
        const val STORE_NAME = "metrics"
        const val METRICS_PING_WORKER_TAG = "mozac_service_glean_metrics_ping_tick"
        const val DUE_HOUR_OF_THE_DAY = 4
    }

    /**
     * The class representing the work to be done by the [WorkManager]. This is used by
     * [schedulePingCollection] for scheduling the collection of the "metrics" ping at
     * the due hour.
     *
     * Please note that this is `inner` to make it easy to call collection functions without
     * too much parameter juggling.
     */
    inner class MetricsPingWorker(context: Context, params: WorkerParameters) : Worker(context, params) {
        override fun doWork(): Result {
            val now = this@MetricsPingScheduler.getCalendarInstance()
            this@MetricsPingScheduler.logger.info("MetricsPingWorker doWork(), now = $now")
            this@MetricsPingScheduler.collectPingAndReschedule(now)
            // We don't expect to fail at collection: we might fail at upload, but that's handled
            // separately by the upload worker.
            return Result.success()
        }
    }

    /**
     * Schedules the metrics ping collection at the due time.
     *
     * @param now the current datetime, a [Calendar] instance.
     * @param sendTheNextCalendarDay whether to schedule collection for the next calendar day
     *        or to attempt to schedule it for the current calendar day. If the latter and
     *        we're overdue for the expected collection time, the task is scheduled for immediate
     *        execution.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun schedulePingCollection(now: Calendar, sendTheNextCalendarDay: Boolean) {
        // Compute how many milliseconds until the next time the metrics ping
        // needs to collect data.
        val millisUntilNextDueTime = getMillisecondsUntilDueTime(sendTheNextCalendarDay, now)

        // Build a work request: we don't use use a `PeriodicWorkRequest`, which
        // is more suitable for this task, because of the inherent drifting. See
        // https://developer.android.com/reference/androidx/work/PeriodicWorkRequest.html
        // for more details.
        val workRequest = OneTimeWorkRequestBuilder<MetricsPingWorker>()
            .addTag(MetricsPingScheduler.METRICS_PING_WORKER_TAG)
            .setInitialDelay(millisUntilNextDueTime, AndroidTimeUnit.MILLISECONDS)
            .build()

        // Enqueue the work request: replace older requests if needed. This is to cover
        // the odd case in which:
        // - glean is killed, but the work request is still there;
        // - glean restarts;
        // - the ping is overdue and is immediately collected at startup;
        // - a new work is scheduled for the next calendar day.
        WorkManager.getInstance().enqueueUniqueWork(
            MetricsPingScheduler.METRICS_PING_WORKER_TAG,
            ExistingWorkPolicy.REPLACE,
            workRequest)
    }

    /**
     * Computes the time in milliseconds until the next metrics ping due time.
     *
     * @param sendTheNextCalendarDay whether or not to return the delay for today or tomorrow's
     *        [dueHourOfTheDay]
     * @param now the current datetime, a [Calendar] instance.
     * @param dueHourOfTheDay the due hour of the day, in the [0, 23] range.
     * @return the milliseconds until the due hour: if current time is before the due
     *         hour, then |dueHour - currentHour| is returned. If it's exactly on that hour,
     *         then 0 is returned. Same if we're past the due hour.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun getMillisecondsUntilDueTime(
        sendTheNextCalendarDay: Boolean,
        now: Calendar,
        dueHourOfTheDay: Int = DUE_HOUR_OF_THE_DAY
    ): Long {
        val nowInMillis = now.timeInMillis
        val dueTime = getDueTimeForToday(now, dueHourOfTheDay)
        val delay = dueTime.timeInMillis - nowInMillis
        return when {
            sendTheNextCalendarDay -> {
                // We're past the `dueHourOfTheDay` in the current calendar day.
                dueTime.add(Calendar.DAY_OF_MONTH, 1)
                dueTime.timeInMillis - nowInMillis
            }
            delay >= 0 -> {
                // The `dueHourOfTheDay` is in the current calendar day.
                // Return the computed delay.
                delay
            }
            else -> {
                // We're overdue and don't want to wait until tomorrow.
                0L
            }
        }
    }

    /**
     * Check if the provided time is after the ping due time.
     *
     * @param now a [Calendar] instance representing the current time.
     * @param dueHourOfTheDay the due hour of the day, in the [0, 23] range.
     * @return true if the current time is after the due hour, false otherwise.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun isAfterDueTime(
        now: Calendar,
        dueHourOfTheDay: Int = DUE_HOUR_OF_THE_DAY
    ): Boolean {
        val nowInMillis = now.timeInMillis
        val dueTime = getDueTimeForToday(now, dueHourOfTheDay)
        return (dueTime.timeInMillis - nowInMillis) < 0
    }

    /**
     * Create a [Calendar] object representing the due time for the current
     * calendar day.
     *
     * @param now a [Calendar] instance representing the current time.
     * @param dueHourOfTheDay the due hour of the day, in the [0, 23] range.
     * @return a new [Calendar] instance representing the due hour for the current calendar day.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun getDueTimeForToday(now: Calendar, dueHourOfTheDay: Int): Calendar {
        val dueTime = now.clone() as Calendar
        dueTime.set(Calendar.HOUR_OF_DAY, dueHourOfTheDay)
        dueTime.set(Calendar.MINUTE, 0)
        dueTime.set(Calendar.SECOND, 0)
        dueTime.set(Calendar.MILLISECOND, 0)
        return dueTime
    }

    /**
     * Performs startup checks to decide when to schedule the next metrics ping
     * collection.
     */
    fun startupCheck() {
        val now = getCalendarInstance()
        val lastSentDate = getLastCollectedDate(now.time)

        // We expect to cover 3 cases here:
        //
        // (1) - the ping was already collected the current calendar day; only schedule
        //       one for collecting the next calendar day at the due time;
        // (2) - the ping was NOT collected the current calendar day, and we're later than
        //       the due time; collect the ping immediately;
        // (3) - the ping was NOT collected the current calendar day, but we still have
        //       some time to the due time; schedule for sending the current calendar day.

        val alreadySentToday = DateUtils.isToday(lastSentDate.time)
        when {
            alreadySentToday -> {
                // The metrics ping was already sent today. Schedule it for the next
                // calendar day. This addresses (1).
                logger.info("The 'metrics' ping was already sent today")
                schedulePingCollection(now, sendTheNextCalendarDay = true)
            }
            // The ping wasn't already sent today. Are we overdue or just waiting for
            // the right time?
            isAfterDueTime(now) -> {
                logger.info("The 'metrics' ping is scheduled for immediate collection")
                // The reason why we're collecting the "metrics" ping in the `Dispatchers.API`
                // context is that we want to make sure no other metric API adds data before
                // the ping is collected. All the exposed metrics API dispatch calls to the
                // engines through the `Dispatchers.API` context, so this ensures we are enqueued
                // before any other recording API call.
                @Suppress("EXPERIMENTAL_API_USAGE")
                Dispatchers.API.launch {
                    // This addresses (2).
                    collectPingAndReschedule(now)
                }
            }
            else -> {
                // This covers (3).
                logger.info("The 'metrics' collection scheduled for the next calendar day")
                schedulePingCollection(now, sendTheNextCalendarDay = false)
            }
        }
    }

    /**
     * Triggers the collection of the "metrics" ping and schedules the
     * next collection.
     *
     * @param now a [Calendar] instance representing the current time.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun collectPingAndReschedule(now: Calendar) {
        logger.info("Collecting the 'metrics' ping, now = $now")
        Glean.sendPings(listOf(STORE_NAME))
        // Update the collection date: we don't really care if we have data or not, let's
        // always update the sent date.
        updateSentDate(getISOTimeString(now, truncateTo = TimeUnit.Day))
        // Reschedule the collection.
        schedulePingCollection(now, sendTheNextCalendarDay = true)
    }

    /**
     * Get the date the metrics ping was last collected.
     *
     * @param defaultDate the [Date] object to return if no valid last sent date can be found
     * @return a [Date] object representing the date the metrics ping was last collected
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun getLastCollectedDate(defaultDate: Date = Date()): Date {
        val loadedDate = try {
            sharedPreferences.getString(LAST_METRICS_PING_SENT_DATETIME, null)
        } catch (e: ClassCastException) {
            logger.error("MetricsPingScheduler last stored ping time was not valid")
            null
        }

        return loadedDate?.let { parseISOTimeString(it) } ?: defaultDate
    }

    /**
     * Update the persisted date when the metrics ping is sent.
     *
     * This is called after sending a metrics ping to timestamp when the last ping was
     * sent in order to maintain the proper interval between pings.
     *
     * @param date the datetime string to store
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun updateSentDate(date: String) {
        sharedPreferences.edit()?.putString(LAST_METRICS_PING_SENT_DATETIME, date)?.apply()
    }

    /**
     * Utility function to mock date creation and ease tests. This is intended
     * to be used only in tests, by overriding the return value with mockito.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun getCalendarInstance(): Calendar = Calendar.getInstance()
}
