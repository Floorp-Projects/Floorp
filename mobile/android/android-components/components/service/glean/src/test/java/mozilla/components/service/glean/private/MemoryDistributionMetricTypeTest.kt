/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.private

import androidx.test.core.app.ApplicationProvider
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.ObsoleteCoroutinesApi
import mozilla.components.service.glean.histogram.FunctionalHistogram
import mozilla.components.service.glean.testing.GleanTestRule
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
class MemoryDistributionMetricTypeTest {

    @get:Rule
    val gleanRule = GleanTestRule(ApplicationProvider.getApplicationContext())

    @Test
    fun `The API saves to its storage engine`() {
        // Define a memory distribution metric which will be stored in "store1"
        val metric = MemoryDistributionMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "memory_distribution",
            sendInPings = listOf("store1"),
            memoryUnit = MemoryUnit.Kilobyte
        )

        // Accumulate a few values
        for (i in 1L..3L) {
            metric.accumulate(i)
        }

        val kb = 1024

        // Check that data was properly recorded.
        assertTrue(metric.testHasValue())
        val snapshot = metric.testGetValue()
        // Check the sum
        assertEquals(1L * kb + 2L * kb + 3L * kb, snapshot.sum)
        // Check that the 1L fell into the first value bucket
        assertEquals(1L, snapshot.values[1024])
        // Check that the 2L fell into the second value bucket
        assertEquals(1L, snapshot.values[2048])
        // Check that the 3L fell into the third value bucket
        assertEquals(1L, snapshot.values[2896])
    }

    @Test
    fun `values are truncated to 1TB`() {
        // Define a memory distribution metric which will be stored in "store1"
        val metric = MemoryDistributionMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "memory_distribution",
            sendInPings = listOf("store1"),
            memoryUnit = MemoryUnit.Gigabyte
        )

        metric.accumulate(2048L)

        // Check that data was properly recorded.
        assertTrue(metric.testHasValue())
        val snapshot = metric.testGetValue()
        // Check the sum
        assertEquals(1L shl 40, snapshot.sum)
        // Check that the 1L fell into 1TB bucket
        assertEquals(1L, snapshot.values[1L shl 40])
    }

    @Test
    fun `disabled memory distributions must not record data`() {
        // Define a memory distribution metric which will be stored in "store1"
        // It's lifetime is set to Lifetime.Ping so it should not record anything.
        val metric = MemoryDistributionMetricType(
            disabled = true,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "memory_distribution",
            sendInPings = listOf("store1"),
            memoryUnit = MemoryUnit.Kilobyte
        )

        metric.accumulate(1L)

        // Check that nothing was recorded.
        assertFalse("MemoryDistributions without a lifetime should not record data.",
            metric.testHasValue())
    }

    @Test(expected = NullPointerException::class)
    fun `testGetValue() throws NullPointerException if nothing is stored`() {
        // Define a memory distribution metric which will be stored in "store1"
        val metric = MemoryDistributionMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "memory_distribution",
            sendInPings = listOf("store1"),
            memoryUnit = MemoryUnit.Kilobyte
        )
        metric.testGetValue()
    }

    @Test
    fun `The API saves to secondary pings`() {
        // Define a memory distribution metric which will be stored in multiple stores
        val metric = MemoryDistributionMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "memory_distribution",
            sendInPings = listOf("store1", "store2", "store3"),
            memoryUnit = MemoryUnit.Kilobyte
        )

        // Accumulate a few values
        for (i in 1L..3L) {
            metric.accumulate(i)
        }

        // Check that data was properly recorded in the second ping.
        assertTrue(metric.testHasValue("store2"))
        val snapshot = metric.testGetValue("store2")
        // Check the sum
        assertEquals(6144L, snapshot.sum)
        // Check that the 1L fell into the first bucket
        assertEquals(1L, snapshot.values[1024])
        // Check that the 2L fell into the second bucket
        assertEquals(1L, snapshot.values[2048])
        // Check that the 3L fell into the third bucket
        assertEquals(1L, snapshot.values[2896])

        // Check that data was properly recorded in the third ping.
        assertTrue(metric.testHasValue("store3"))
        val snapshot2 = metric.testGetValue("store3")
        // Check the sum
        assertEquals(6144L, snapshot2.sum)
        // Check that the 1L fell into the first bucket
        assertEquals(1L, snapshot2.values[1024])
        // Check that the 2L fell into the second bucket
        assertEquals(1L, snapshot2.values[2048])
        // Check that the 3L fell into the third bucket
        assertEquals(1L, snapshot2.values[2896])
    }

    @Test
    fun `The accumulateSamples API correctly stores memory values`() {
        // Define a memory distribution metric which will be stored in multiple stores
        val metric = MemoryDistributionMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "memory_distribution_samples",
            sendInPings = listOf("store1"),
            memoryUnit = MemoryUnit.Kilobyte
        )

        // Accumulate a few values
        val testSamples = (1L..3L).toList().toLongArray()
        metric.accumulateSamples(testSamples)

        val kb = 1024L

        // Check that data was properly recorded in the second ping.
        assertTrue(metric.testHasValue("store1"))
        val snapshot = metric.testGetValue("store1")
        // Check the sum
        assertEquals(6L * kb, snapshot.sum)
        // Check that the 1L fell into the first bucket
        assertEquals(
            1L, snapshot.values[FunctionalHistogram.sampleToBucketMinimum(1 * kb)]
        )
        // Check that the 2L fell into the second bucket
        assertEquals(
            1L, snapshot.values[FunctionalHistogram.sampleToBucketMinimum(2 * kb)]
        )
        // Check that the 3L fell into the third bucket
        assertEquals(
            1L, snapshot.values[FunctionalHistogram.sampleToBucketMinimum(3 * kb)]
        )
    }
}
