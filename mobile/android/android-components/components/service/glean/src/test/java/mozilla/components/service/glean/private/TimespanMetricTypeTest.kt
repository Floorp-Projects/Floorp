/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.private

import androidx.test.core.app.ApplicationProvider
import mozilla.components.service.glean.error.ErrorRecording.ErrorType
import mozilla.components.service.glean.error.ErrorRecording.testGetNumRecordedErrors
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.service.glean.timing.TimingManager
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class TimespanMetricTypeTest {

    @get:Rule
    val gleanRule = GleanTestRule(ApplicationProvider.getApplicationContext())

    @Test
    fun `The API must record to its storage engine`() {
        // Define a timespan metric, which will be stored in "store1"
        val metric = TimespanMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "timespan_metric",
            sendInPings = listOf("store1"),
            timeUnit = TimeUnit.Millisecond
        )

        // Record a timespan.
        metric.start()
        metric.stop()

        // Check that data was properly recorded.
        assertTrue(metric.testHasValue())
        assertTrue(metric.testGetValue() >= 0)
    }

    @Test
    fun `The API should not record if the metric is disabled`() {
        // Define a timespan metric, which will be stored in "store1"
        val metric = TimespanMetricType(
            disabled = true,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "timespan_metric",
            sendInPings = listOf("store1"),
            timeUnit = TimeUnit.Millisecond
        )

        // Record a timespan.
        metric.start()
        metric.stop()

        // Let's also call cancel() to make sure it's a no-op.
        metric.cancel()

        // Check that data was not recorded.
        assertFalse("The API should not record a counter if metric is disabled",
            metric.testHasValue())
    }

    @Test
    fun `The API must correctly cancel`() {
        // Define a timespan metric, which will be stored in "store1"
        val metric = TimespanMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "timespan_metric",
            sendInPings = listOf("store1"),
            timeUnit = TimeUnit.Millisecond
        )

        // Record a timespan.
        metric.start()
        metric.cancel()
        metric.stop()

        // Check that data was not recorded.
        assertFalse("The API should not record a counter if metric is cancelled",
            metric.testHasValue())
        assertEquals(1, testGetNumRecordedErrors(metric, ErrorType.InvalidValue))
    }

    @Test
    fun `The API must correctly clear the timer state on stop()`() {
        // Define a timespan metric, which will be stored in "store1"
        val metric = TimespanMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "timespan_metric",
            sendInPings = listOf("store1"),
            timeUnit = TimeUnit.Millisecond
        )

        // Record a timespan.
        TimingManager.getElapsedNanos = { 0 }
        metric.start()
        TimingManager.getElapsedNanos = { 5000000 }
        metric.stop()

        // Attempt to record again.
        TimingManager.getElapsedNanos = { 0 }
        metric.start()
        TimingManager.getElapsedNanos = { 10000000 }
        metric.stop()

        // Check only the first chunk was recorded.
        assertEquals(5, metric.testGetValue())
        // And that an attempt to record the second time was tracked.
        assertEquals(1, testGetNumRecordedErrors(metric, ErrorType.InvalidValue))
    }

    @Test(expected = NullPointerException::class)
    fun `testGetValue() throws NullPointerException if nothing is stored`() {
        val metric = TimespanMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "timespan_metric",
            sendInPings = listOf("store1"),
            timeUnit = TimeUnit.Millisecond
        )
        metric.testGetValue()
    }

    @Test
    fun `The API saves to secondary pings`() {
        // Define a timespan metric, which will be stored in "store1" and "store2"
        val metric = TimespanMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "timespan_metric",
            sendInPings = listOf("store1", "store2"),
            timeUnit = TimeUnit.Millisecond
        )

        // Record a timespan.
        metric.start()
        metric.stop()

        // Check that data was properly recorded in the second ping.
        assertTrue(metric.testHasValue("store2"))
        assertTrue(metric.testGetValue("store2") >= 0)
    }

    @Test
    fun `Records an error if started twice`() {
        // Define a timespan metric, which will be stored in "store1" and "store2"
        val metric = TimespanMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "timespan_metric",
            sendInPings = listOf("store1", "store2"),
            timeUnit = TimeUnit.Millisecond
        )

        // Record a timespan.
        metric.start()
        metric.start()
        metric.stop()

        // Check that data was properly recorded in the second ping.
        assertTrue(metric.testHasValue("store2"))
        assertTrue(metric.testGetValue("store2") >= 0)
        assertEquals(1, testGetNumRecordedErrors(metric, ErrorType.InvalidValue))
    }

    @Test
    fun `Value unchanged if stopped twice`() {
        // Define a timespan metric, which will be stored in "store1" and "store2"
        val metric = TimespanMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "timespan_metric",
            sendInPings = listOf("store1"),
            timeUnit = TimeUnit.Nanosecond
        )

        // Record a timespan.
        metric.start()
        metric.stop()
        assertTrue(metric.testHasValue())
        val value = metric.testGetValue()

        metric.stop()

        assertEquals(value, metric.testGetValue())
    }

    @Test
    fun `test setRawNanos`() {
        val timespanNanos = 6 * 1000000000L

        val metric = TimespanMetricType(
            false,
            "telemetry",
            Lifetime.Ping,
            "explicit_timespan",
            listOf("store1"),
            timeUnit = TimeUnit.Second
        )

        metric.setRawNanos(timespanNanos)
        assertEquals(6, metric.testGetValue())
    }

    @Test
    fun `test setRawNanos followed by other API`() {
        val timespanNanos = 6 * 1000000000L

        val metric = TimespanMetricType(
            false,
            "telemetry",
            Lifetime.Ping,
            "explicit_timespan_1",
            listOf("store1"),
            timeUnit = TimeUnit.Second
        )

        metric.setRawNanos(timespanNanos)
        assertEquals(6, metric.testGetValue())

        TimingManager.getElapsedNanos = { 0 }
        metric.start()
        TimingManager.getElapsedNanos = { 50 }
        metric.stop()
        val value = metric.testGetValue()
        assertEquals(6, value)
    }

    @Test
    fun `setRawNanos does not overwrite value`() {
        val timespanNanos = 6 * 1000000000L

        val metric = TimespanMetricType(
            false,
            "telemetry",
            Lifetime.Ping,
            "explicit_timespan_1",
            listOf("store1"),
            timeUnit = TimeUnit.Second
        )

        metric.start()
        metric.stop()
        val value = metric.testGetValue()

        metric.setRawNanos(timespanNanos)

        assertEquals(value, metric.testGetValue())
    }

    @Test
    fun `setRawNanos does nothing when timer is running`() {
        val timespanNanos = 1000000000L

        val metric = TimespanMetricType(
            false,
            "telemetry",
            Lifetime.Ping,
            "explicit_timespan",
            listOf("store1"),
            timeUnit = TimeUnit.Second
        )

        metric.start()
        metric.setRawNanos(timespanNanos)
        metric.stop()

        assertNotEquals(timespanNanos, metric.testGetValue())
        assertEquals(1, testGetNumRecordedErrors(metric, ErrorType.InvalidValue))
    }
}
