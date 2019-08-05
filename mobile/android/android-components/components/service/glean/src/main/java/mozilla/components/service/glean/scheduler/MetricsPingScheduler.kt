/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.scheduler

import android.content.Context
import android.content.SharedPreferences
import androidx.annotation.VisibleForTesting
import android.text.format.DateUtils
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleObserver
import androidx.lifecycle.OnLifecycleEvent
import androidx.lifecycle.ProcessLifecycleOwner
import androidx.work.ExistingWorkPolicy
import androidx.work.OneTimeWorkRequestBuilder
import androidx.work.Worker
import androidx.work.WorkManager
import androidx.work.WorkerParameters
import mozilla.components.service.glean.Dispatchers
import mozilla.components.service.glean.Glean
import mozilla.components.service.glean.GleanMetrics.Pings
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.service.glean.utils.getISOTimeString
import mozilla.components.service.glean.utils.parseISOTimeString
import mozilla.components.service.glean.private.TimeUnit
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
 *
 * The scheduler also makes use of the [LifecycleObserver] in order to correctly schedule
 * the [MetricsPingWorker]
 */
@Suppress("TooManyFunctions")
internal class MetricsPingScheduler(val applicationContext: Context) : LifecycleObserver {
    private val logger = Logger("glean/MetricsPingScheduler")
    internal val sharedPreferences: SharedPreferences by lazy {
        applicationContext.getSharedPreferences(this.javaClass.canonicalName, Context.MODE_PRIVATE)
    }

    companion object {
        const val LAST_METRICS_PING_SENT_DATETIME = "last_metrics_ping_iso_datetime"
        const val DUE_HOUR_OF_THE_DAY = 4
        @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
        internal var isInForeground = false
    }

    init {
        ProcessLifecycleOwner.get().lifecycle.addObserver(this)
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
        logger.debug("Scheduling the 'metrics' ping in ${millisUntilNextDueTime}ms")

        // Build a work request: we don't use use a `PeriodicWorkRequest`, which
        // is more suitable for this task, because of the inherent drifting. See
        // https://developer.android.com/reference/androidx/work/PeriodicWorkRequest.html
        // for more details.
        val workRequest = OneTimeWorkRequestBuilder<MetricsPingWorker>()
            .addTag(MetricsPingWorker.TAG)
            .setInitialDelay(millisUntilNextDueTime, AndroidTimeUnit.MILLISECONDS)
            .build()

        // Enqueue the work request: replace older requests if needed. This is to cover
        // the odd case in which:
        // - Glean is killed, but the work request is still there;
        // - Glean restarts;
        // - the ping is overdue and is immediately collected at startup;
        // - a new work is scheduled for the next calendar day.
        WorkManager.getInstance().enqueueUniqueWork(
            MetricsPingWorker.TAG,
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
    fun schedule() {
        val now = getCalendarInstance()
        val lastSentDate = getLastCollectedDate()

        if (lastSentDate != null) {
            logger.debug("The 'metrics' ping was last sent on $lastSentDate")
        }

        // We expect to cover 3 cases here:
        //
        // (1) - the ping was already collected the current calendar day; only schedule
        //       one for collecting the next calendar day at the due time;
        // (2) - the ping was NOT collected the current calendar day, and we're later than
        //       the due time; collect the ping immediately;
        // (3) - the ping was NOT collected the current calendar day, but we still have
        //       some time to the due time; schedule for sending the current calendar day.

        val alreadySentToday = (lastSentDate != null) && DateUtils.isToday(lastSentDate.time)
        when {
            alreadySentToday -> {
                // The metrics ping was already sent today. Schedule it for the next
                // calendar day. This addresses (1).
                logger.info("The 'metrics' ping was already sent today, ${now.time}.")
                schedulePingCollection(now, sendTheNextCalendarDay = true)
            }
            // The ping wasn't already sent today. Are we overdue or just waiting for
            // the right time?
            isAfterDueTime(now) -> {
                logger.info("The 'metrics' ping is scheduled for immediate collection, ${now.time}")
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
                logger.info("The 'metrics' collection is scheduled for today, ${now.time}")
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
        logger.info("Collecting the 'metrics' ping, now = ${now.time}")
        Pings.metrics.send()
        // Update the collection date: we don't really care if we have data or not, let's
        // always update the sent date.
        updateSentDate(getISOTimeString(now, truncateTo = TimeUnit.Day))

        // Reschedule the collection if we are in the foreground so that any metrics collected after
        // this are sent in the next window.  If we are in the background, then we may stay there
        // until the app is killed so we shouldn't reschedule unless the app is foregrounded again
        // (see GleanLifecycleObserver).
        if (isInForeground) {
            schedulePingCollection(now, sendTheNextCalendarDay = true)
        }
    }

    /**
     * Get the date the metrics ping was last collected.
     *
     * @return a [Date] object representing the date the metrics ping was last collected, or
     *         null if no metrics ping was previously collected.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    internal fun getLastCollectedDate(): Date? {
        val loadedDate = try {
            sharedPreferences.getString(LAST_METRICS_PING_SENT_DATETIME, null)
        } catch (e: ClassCastException) {
            null
        }

        if (loadedDate == null) {
            logger.error("MetricsPingScheduler last stored ping time was not valid")
        }

        return loadedDate?.let { parseISOTimeString(it) }
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

    /**
     * Update flag to show we are no longer in the foreground.
     */
    @OnLifecycleEvent(Lifecycle.Event.ON_STOP)
    fun onEnterBackground() {
        isInForeground = false
    }

    /**
     * Update the flag to indicate we are moving to the foreground, and if Glean is initialized we
     * will check to see if the metrics ping needs scheduled for collection.
     */
    @OnLifecycleEvent(Lifecycle.Event.ON_START)
    fun onEnterForeground() {
        isInForeground = true

        // We check for the metrics ping schedule here because the app could have been in the
        // background and resumed in which case Glean would already be initialized but we still need
        // to perform the check to determine whether or not to collect and schedule the metrics ping.
        // If this is the first ON_START event since the app was launched, Glean wouldn't be
        // initialized yet.
        if (Glean.isInitialized()) {
            schedule()
        }
    }
}

/**
 * The class representing the work to be done by the [WorkManager]. This is used by
 * [MetricsPingScheduler.schedulePingCollection] for scheduling the collection of the
 * "metrics" ping at the due hour.
 */
internal class MetricsPingWorker(context: Context, params: WorkerParameters) : Worker(context, params) {
    private val logger = Logger("glean/MetricsPingWorker")

    companion object {
        const val TAG = "mozac_service_glean_metrics_ping_tick"
    }

    override fun doWork(): Result {
        // This is getting the instance of the MetricsPingScheduler class instantiated by
        // the [Glean] singleton. This is ugly. There are a few alternatives to this:
        //
        // 1. provide a custom WorkerFactory to the WorkManager; however this would require
        //    us to prevent the application from initializing the WorkManager at startup in
        //    order to manually init it ourselves and feed in our custom configuration with
        //    the new factory.
        // 2. make most functions of MetricsPingScheduler static and allow for calling them
        //    from this worker; this makes testing much more complicated, due to the restrictions
        //    related to static functions when testing.
        val metricsScheduler = Glean.metricsPingScheduler
        // Perform the actual work.
        val now = metricsScheduler.getCalendarInstance()
        logger.debug("MetricsPingWorker doWork(), now = ${now.time}")
        metricsScheduler.collectPingAndReschedule(now)
        // We don't expect to fail at collection: we might fail at upload, but that's handled
        // separately by the upload worker.
        return Result.success()
    }
}
