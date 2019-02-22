/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.scheduler

import android.content.Context
import android.content.SharedPreferences
import mozilla.components.support.base.log.logger.Logger
import java.util.concurrent.TimeUnit

/**
 * MetricsPingScheduler facilitates sending and scheduling of metrics pings when the application
 * enters the background state (see [GleanLifecycleObserver]), but the ping is only sent if the
 * scheduled time interval [PING_INTERVAL_HOURS] has elapsed.
 */
internal class MetricsPingScheduler(val applicationContext: Context) {
    private val logger = Logger("glean/MetricsPingScheduler")
    internal val sharedPreferences: SharedPreferences by lazy { getMetricsPrefs() }

    companion object {
        const val LAST_METRICS_PING_TIMESTAMP_KEY = "last_metrics_ping_timestamp"
        const val STORE_NAME = "metrics"
        const val PING_INTERVAL_HOURS = 24L
    }

    /**
     * Check to see if a metrics ping can be sent.
     *
     * This will compare the current system time to the last ping time stored in [SharedPreferences]
     * to determine whether [PING_INTERVAL_HOURS] has elapsed.
     *
     * @return true if the time interval has elapsed or if this is the first attempt to send a
     *         metrics ping, otherwise false.
     */
    fun canSendPing(): Boolean {
        val lastPingTime = try {
            sharedPreferences.getLong(LAST_METRICS_PING_TIMESTAMP_KEY, 0L)
        } catch (e: ClassCastException) {
            // Intentionally doing nothing here, if LAST_METRICS_PING_TIMESTAMP_KEY's value is
            // not a Long, then we will let lastPingTime be 0L for the purposes of comparison in
            // order to allow the ping to be sent immediately
            logger.error("MetricsPingScheduler last ping time stored was not a Long value")
            0L
        }

        val currentTime = System.currentTimeMillis()

        if (lastPingTime == 0L ||
            currentTime - lastPingTime >= TimeUnit.HOURS.toMillis(PING_INTERVAL_HOURS)) {
            return true
        }

        return false
    }

    /**
     * Update the persisted timestamp when the metrics ping is sent.
     *
     * This is called after sending a metrics ping in order to timestamp when the last ping was
     * sent in order to maintain the proper interval between pings. See [canSendPing].
     *
     * @param timestamp The timestamp to store, which defaults to [System.currentTimeMillis].
     */
    fun updateSentTimestamp(timestamp: Long = System.currentTimeMillis()) {
        sharedPreferences.edit()?.putLong(LAST_METRICS_PING_TIMESTAMP_KEY, timestamp)?.apply()
    }

    /**
     * Initializes the [SharedPreferences] where persisted metrics ping information is stored.
     */
    private fun getMetricsPrefs(): SharedPreferences {
        return applicationContext.getSharedPreferences(this.javaClass.canonicalName, Context.MODE_PRIVATE)
    }

    /**
     * Test only function to clear stored data
     */
    internal fun clearSchedulerData() {
        sharedPreferences.edit()?.clear()?.apply()
    }
}
