/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.scheduler

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import java.util.concurrent.TimeUnit

@RunWith(RobolectricTestRunner::class)
class MetricsPingSchedulerTest {
    // NOTE: Using a "real" ApplicationContext here so that it will have
    // a working SharedPreferences implementation
    private val metricsPingScheduler = MetricsPingScheduler(
        ApplicationProvider.getApplicationContext<Context>()
    )

    @Before
    fun setup() {
        metricsPingScheduler.clearSchedulerData()
    }

    @Test
    fun `metrics ping canSendPing() returns true if no timestamp was recorded in shared prefs`() {
        assertTrue(metricsPingScheduler.canSendPing())
    }

    @Test
    fun `metrics ping canSendPing() returns true if invalid data type stored in key`() {
        metricsPingScheduler.sharedPreferences.edit()
                .putString(MetricsPingScheduler.LAST_METRICS_PING_TIMESTAMP_KEY, "Not a Long value!")
                .apply()

        // This should still assert as true since the scheduler will catch an exception when
        // retrieving the value for the LAST_METRICS_PING_TIMESTAMP_KEY that it expects to be a
        // Long type.  Since it finds a string, the exception is thrown and the scheduler should
        // treat it just like if no value had been stored at all.
        assertTrue(metricsPingScheduler.canSendPing())
    }

    @Test
    fun `MetricsPingScheduler updatePingTimestamp() correctly stores values`() {
        // Try storing an arbitrary timestamp to see that it is stored correctly
        metricsPingScheduler.updateSentTimestamp(1234L)
        val ts = metricsPingScheduler.sharedPreferences
                .getLong(MetricsPingScheduler.LAST_METRICS_PING_TIMESTAMP_KEY, 0)
        assertEquals(ts, 1234L)
    }

    @Test
    fun `MetricsPingScheduler correctly initializes it's SharedPreferences`() {
        // This will be null if the lazy initialization failed for some reason
        assertNotNull(metricsPingScheduler.sharedPreferences)
    }

    @Test
    fun `metrics ping canSendPing() returns false if interval hasn't elapsed`() {
        // Store the current system time as the ping timestamp to compare to
        metricsPingScheduler.updateSentTimestamp()

        assertFalse(metricsPingScheduler.canSendPing())
    }

    @Test
    fun `metrics ping canSendPing() returns true if interval has elapsed`() {
        // Store a ping timestamp 1 hour greater than the interval so that the canSendPing()
        // function should return true
        metricsPingScheduler.updateSentTimestamp(
                System.currentTimeMillis() -
                        TimeUnit.HOURS.toMillis(MetricsPingScheduler.PING_INTERVAL_HOURS + 1))

        assertTrue(metricsPingScheduler.canSendPing())
    }
}
