/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.private

import androidx.test.core.app.ApplicationProvider
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.ObsoleteCoroutinesApi
import mozilla.components.service.glean.storages.TimingDistributionData
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.service.glean.timing.TimingManager
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Assert.assertFalse
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import java.lang.NullPointerException

@ObsoleteCoroutinesApi
@ExperimentalCoroutinesApi
@RunWith(RobolectricTestRunner::class)
class TimingDistributionMetricTypeTest {

    @get:Rule
    val gleanRule = GleanTestRule(ApplicationProvider.getApplicationContext())

    @After
    fun reset() {
        TimingManager.testResetTimeSource()
    }

    @Test
    fun `The API saves to its storage engine`() {
        // Define a timing distribution metric which will be stored in "store1"
        val metric = TimingDistributionMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "timing_distribution",
            sendInPings = listOf("store1"),
            timeUnit = TimeUnit.Millisecond
        )

        // Accumulate a few values
        for (i in 1L..3L) {
            TimingManager.getElapsedNanos = { 0 }
            val timerId = metric.start()
            TimingManager.getElapsedNanos = { i }
            metric.stopAndAccumulate(timerId)
        }

        // Check that data was properly recorded.
        assertTrue(metric.testHasValue())
        val snapshot = metric.testGetValue()
        // Check the sum
        assertEquals(6L, snapshot.sum)
        // Check that the 1L fell into the first value bucket
        assertEquals(1L, snapshot.values[1])
        // Check that the 2L fell into the second value bucket
        assertEquals(1L, snapshot.values[2])
        // Check that the 3L fell into the third value bucket
        assertEquals(1L, snapshot.values[3])
    }

    @Test
    fun `disabled timing distributions must not record data`() {
        // Define a timing distribution metric which will be stored in "store1"
        // It's lifetime is set to Lifetime.Ping so it should not record anything.
        val metric = TimingDistributionMetricType(
            disabled = true,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "timing_distribution",
            sendInPings = listOf("store1"),
            timeUnit = TimeUnit.Millisecond
        )

        // Attempt to store the timespan using set
        TimingManager.getElapsedNanos = { 0 }
        val timerId = metric.start()
        TimingManager.getElapsedNanos = { 1 }
        metric.stopAndAccumulate(timerId)

        // Check that nothing was recorded.
        assertFalse("TimingDistributions without a lifetime should not record data.",
            metric.testHasValue())
    }

    @Test(expected = NullPointerException::class)
    fun `testGetValue() throws NullPointerException if nothing is stored`() {
        // Define a timing distribution metric which will be stored in "store1"
        val metric = TimingDistributionMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "timing_distribution",
            sendInPings = listOf("store1"),
            timeUnit = TimeUnit.Millisecond
        )
        metric.testGetValue()
    }

    @Test
    fun `The API saves to secondary pings`() {
        // Define a timing distribution metric which will be stored in multiple stores
        val metric = TimingDistributionMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "timing_distribution",
            sendInPings = listOf("store1", "store2", "store3"),
            timeUnit = TimeUnit.Millisecond
        )

        // Accumulate a few values
        for (i in 1L..3L) {
            TimingManager.getElapsedNanos = { 0 }
            val timerId = metric.start()
            TimingManager.getElapsedNanos = { i }
            metric.stopAndAccumulate(timerId)
        }

        // Check that data was properly recorded in the second ping.
        assertTrue(metric.testHasValue("store2"))
        val snapshot = metric.testGetValue("store2")
        // Check the sum
        assertEquals(6L, snapshot.sum)
        // Check that the 1L fell into the first bucket
        assertEquals(1L, snapshot.values[1])
        // Check that the 2L fell into the second bucket
        assertEquals(1L, snapshot.values[2])
        // Check that the 3L fell into the third bucket
        assertEquals(1L, snapshot.values[3])

        // Check that data was properly recorded in the third ping.
        assertTrue(metric.testHasValue("store3"))
        val snapshot2 = metric.testGetValue("store3")
        // Check the sum
        assertEquals(6L, snapshot2.sum)
        // Check that the 1L fell into the first bucket
        assertEquals(1L, snapshot2.values[1])
        // Check that the 2L fell into the second bucket
        assertEquals(1L, snapshot2.values[2])
        // Check that the 3L fell into the third bucket
        assertEquals(1L, snapshot2.values[3])
    }

    @Test
    fun `The accumulateSamples API correctly stores timing values`() {
        // Define a timing distribution metric which will be stored in multiple stores
        val metric = TimingDistributionMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "timing_distribution_samples",
            sendInPings = listOf("store1"),
            timeUnit = TimeUnit.Second
        )

        // Accumulate a few values
        val testSamples = (1L..3L).toList().toLongArray()
        metric.accumulateSamples(testSamples)

        val secondsToNanos = 1000L * 1000L * 1000L

        // Check that data was properly recorded in the second ping.
        assertTrue(metric.testHasValue("store1"))
        val snapshot = metric.testGetValue("store1")
        // Check the sum
        assertEquals(6L * secondsToNanos, snapshot.sum)
        // Check that the 1L fell into the first bucket
        assertEquals(
            1L, snapshot.values[TimingDistributionData.sampleToBucketMinimum(1 * secondsToNanos)]
        )
        // Check that the 2L fell into the second bucket
        assertEquals(
            1L, snapshot.values[TimingDistributionData.sampleToBucketMinimum(2 * secondsToNanos)]
        )
        // Check that the 3L fell into the third bucket
        assertEquals(
            1L, snapshot.values[TimingDistributionData.sampleToBucketMinimum(3 * secondsToNanos)]
        )
    }
}
