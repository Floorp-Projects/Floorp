/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.timing

import android.os.SystemClock
import android.support.annotation.VisibleForTesting
import mozilla.components.service.glean.error.ErrorRecording
import mozilla.components.service.glean.error.ErrorRecording.recordError
import mozilla.components.service.glean.private.CommonMetricData
import mozilla.components.support.base.log.logger.Logger
import java.util.WeakHashMap

/**
 * A class to record timing intervals associated with a given arbitrary object.
 *
 * A timings are recorded and returned in nanoseconds.
 */
internal object TimingManager {
    private val logger = Logger("glean/TimingManager")

    /**
     * A map that stores the start times of running timers.
     *
     * A [WeakHashMap] is used so that we don't unintentionally leak memory
     * by keeping references to otherwise destroyed objects around.
     */
    val uncommittedStartTimes = mutableMapOf<String, WeakHashMap<Any, Long>>()

    /**
     * Helper function used for getting the elapsed time, since the process
     * started, using a monotonic clock.
     * We need to have this as an helper so that we can override it in tests.
     *
     * @return the time, in nanoseconds, since the process started.
     */
    internal var getElapsedNanos = { SystemClock.elapsedRealtimeNanos() }

    /**
     * Start tracking time associated with the provided object. This records an
     * error if it’s already tracking time (i.e. start was already called with
     * no corresponding [stop]): in that case, the original start time will be
     * preserved.
     *
     * @param metricData The metric managing the timing. Used for error reporting.
     * @param timerId The object to associate with this timing.
     */
    public fun start(metricData: CommonMetricData, timerId: Any) {
        val startTime = getElapsedNanos()

        synchronized(this) {
            val metricName = metricData.identifier
            uncommittedStartTimes[metricName]?.let { metricTimings ->
                if (timerId in metricTimings) {
                    recordError(
                        metricData,
                        ErrorRecording.ErrorType.InvalidValue,
                        "Timespan already started",
                        logger
                    )
                    return
                }
            }

            uncommittedStartTimes.getOrPut(metricName, { WeakHashMap<Any, Long>() })[timerId] = startTime
        }
    }

    /**
     * Stop tracking time for the associated object. This will record
     * an error if no [start] was called.
     *
     * @param metricData The metric managing the timing. Used for error reporting.
     * @param timerId The object to associate with this timing.
     * @return The length of the timespan, in nanoseconds, or null if called on a stopped timer.
     */
    public fun stop(metricData: CommonMetricData, timerId: Any): Long? {
        val stopTime = getElapsedNanos()

        return synchronized(this) {
            val metricName = metricData.identifier
            uncommittedStartTimes[metricName]?.remove(timerId)?.let { startTime ->
                stopTime - startTime
            } ?: run {
                recordError(
                    metricData,
                    ErrorRecording.ErrorType.InvalidValue,
                    "Timespan not running",
                    logger
                )
                null
            }
        }
    }

    /**
     * Abort a previous [start] call. No error is recorded if no [start] was called.
     *
     * @param timerId The object to associate with this timing.
     */
    public fun cancel(metricData: CommonMetricData, timerId: Any) {
        synchronized(this) {
            val metricName = metricData.identifier
            uncommittedStartTimes[metricName]?.remove(timerId)
        }
    }

    /**
     * Reset the source of timing back to the default after it's been overridden. Use
     * for testing only.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    internal fun testResetTimeSource() {
        TimingManager.getElapsedNanos = { SystemClock.elapsedRealtimeNanos() }
    }
}
