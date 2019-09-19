/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.storages

import android.content.Context
import android.content.SharedPreferences
import androidx.test.core.app.ApplicationProvider
import kotlinx.coroutines.runBlocking
import mozilla.components.service.glean.collectAndCheckPingSchema
import mozilla.components.service.glean.private.Lifetime
import mozilla.components.service.glean.private.TimeUnit
import mozilla.components.service.glean.private.TimingDistributionMetricType
import mozilla.components.service.glean.error.ErrorRecording
import mozilla.components.service.glean.histogram.FunctionalHistogram
import mozilla.components.service.glean.private.PingType
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.service.glean.timing.TimingManager
import org.junit.After
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Assert.assertEquals
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers
import org.mockito.Mockito
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class TimingDistributionsStorageEngineTest {

    @get:Rule
    val gleanRule = GleanTestRule(ApplicationProvider.getApplicationContext())

    @After
    fun reset() {
        TimingManager.testResetTimeSource()
    }

    @Test
    fun `accumulate() properly updates the values in all stores`() {
        val storeNames = listOf("store1", "store2")

        val metric = TimingDistributionMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "test_timing_distribution",
            sendInPings = storeNames,
            timeUnit = TimeUnit.Millisecond
        )

        // Create a sample that will fall into the underflow bucket (bucket '0') so we can easily
        // find it
        val sample = 1L
        TimingDistributionsStorageEngine.accumulate(
            metricData = metric,
            sample = sample
        )

        // Check that the data was correctly set in each store.
        for (storeName in storeNames) {
            val snapshot = TimingDistributionsStorageEngine.getSnapshot(
                storeName = storeName,
                clearStore = true
            )
            assertEquals(1, snapshot!!.size)
            assertEquals(1L, snapshot["telemetry.test_timing_distribution"]?.values!![1])
        }
    }

    @Test
    fun `deserializer should correctly parse timing distributions`() {
        val td = FunctionalHistogram(
            TimingDistributionsStorageEngineImplementation.LOG_BASE,
            TimingDistributionsStorageEngineImplementation.BUCKETS_PER_MAGNITUDE
        )

        val persistedSample = mapOf(
            "store1#telemetry.invalid_string" to "invalid_string",
            "store1#telemetry.invalid_bool" to false,
            "store1#telemetry.null" to null,
            "store1#telemetry.invalid_int" to -1,
            "store1#telemetry.invalid_list" to listOf("1", "2", "3"),
            "store1#telemetry.invalid_int_list" to "[1,2,3]",
            "store1#telemetry.invalid_td_values" to "{\"log_base\":2.0,\"buckets_per_magnitude\":8.0,\"values\":{\"0\": \"nope\"},\"sum\":0}",
            "store1#telemetry.invalid_td_sum" to "{\"log_base\":2.0,\"buckets_per_magnitude\":8.0,\"values\":{},\"sum\":\"nope\"}",
            "store1#telemetry.test_timing_distribution" to td.toJsonObject().toString()
        )

        val storageEngine = TimingDistributionsStorageEngineImplementation()

        // Create a fake application context that will be used to load our data.
        val context = Mockito.mock(Context::class.java)
        val sharedPreferences = Mockito.mock(SharedPreferences::class.java)
        Mockito.`when`(sharedPreferences.all).thenAnswer { persistedSample }
        Mockito.`when`(context.getSharedPreferences(
            ArgumentMatchers.eq(storageEngine::class.java.canonicalName),
            ArgumentMatchers.eq(Context.MODE_PRIVATE)
        )).thenReturn(sharedPreferences)
        Mockito.`when`(context.getSharedPreferences(
            ArgumentMatchers.eq("${storageEngine::class.java.canonicalName}.PingLifetime"),
            ArgumentMatchers.eq(Context.MODE_PRIVATE)
        )).thenReturn(ApplicationProvider.getApplicationContext<Context>()
            .getSharedPreferences("${storageEngine::class.java.canonicalName}.PingLifetime",
                Context.MODE_PRIVATE))

        storageEngine.applicationContext = context
        val snapshot = storageEngine.getSnapshot(storeName = "store1", clearStore = true)
        assertEquals(1, snapshot!!.size)
        assertEquals(td.toJsonObject().toString(),
            snapshot["telemetry.test_timing_distribution"]?.toJsonObject().toString())
    }

    @Test
    fun `serializer should serialize timing distribution that matches schema`() {
        val ping1 = PingType("store1", true)

        val metric = TimingDistributionMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.User,
            name = "test_timing_distribution",
            sendInPings = listOf("store1"),
            timeUnit = TimeUnit.Millisecond
        )

        runBlocking {
            TimingManager.getElapsedNanos = { 0 }
            val id = metric.start()
            TimingManager.getElapsedNanos = { 1000000 }
            metric.stopAndAccumulate(id)
        }

        collectAndCheckPingSchema(ping1)
    }

    @Test
    fun `serializer should correctly serialize timing distributions`() {
        run {
            val storageEngine = TimingDistributionsStorageEngineImplementation()
            storageEngine.applicationContext = ApplicationProvider.getApplicationContext()

            val metric = TimingDistributionMetricType(
                disabled = false,
                category = "telemetry",
                lifetime = Lifetime.User,
                name = "test_timing_distribution",
                sendInPings = listOf("store1", "store2"),
                timeUnit = TimeUnit.Millisecond
            )

            // Using the FunctionalHistogram object here to easily turn the object into JSON
            // for comparison purposes.
            val td = FunctionalHistogram(
                TimingDistributionsStorageEngineImplementation.LOG_BASE,
                TimingDistributionsStorageEngineImplementation.BUCKETS_PER_MAGNITUDE
            )
            td.accumulate(1000000L)

            runBlocking {
                storageEngine.accumulate(
                    metricData = metric,
                    sample = 1000000L
                )
            }

            // Get snapshot from store1
            val json = storageEngine.getSnapshotAsJSON("store1", true)
            // Check for correct JSON serialization
            assertEquals("{\"${metric.identifier}\":${td.toJsonPayloadObject()}}",
                json.toString()
            )
        }

        // Create a new instance of storage engine to verify serialization to storage rather than
        // to the cache
        run {
            val storageEngine = TimingDistributionsStorageEngineImplementation()
            storageEngine.applicationContext = ApplicationProvider.getApplicationContext()

            val td = FunctionalHistogram(
                TimingDistributionsStorageEngineImplementation.LOG_BASE,
                TimingDistributionsStorageEngineImplementation.BUCKETS_PER_MAGNITUDE
            )
            td.accumulate(1000000L)

            // Get snapshot from store1
            val json = storageEngine.getSnapshotAsJSON("store1", true)
            // Check for correct JSON serialization
            assertEquals("{\"telemetry.test_timing_distribution\":${td.toJsonPayloadObject()}}",
                json.toString()
            )
        }
    }

    @Test
    fun `timing distributions must not accumulate negative values`() {
        // Define a timing distribution metric, which will be stored in "store1".
        val metric = TimingDistributionMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.User,
            name = "test_timing_distribution",
            sendInPings = listOf("store1"),
            timeUnit = TimeUnit.Millisecond
        )

        // Attempt to accumulate a negative sample
        TimingManager.getElapsedNanos = { 0 }
        val timerId = metric.start()
        TimingManager.getElapsedNanos = { -1 }
        metric.stopAndAccumulate(timerId)
        // Check that nothing was recorded.
        assertFalse("Timing distributions must not accumulate negative values",
            metric.testHasValue())

        // Make sure that the errors have been recorded
        assertEquals("Accumulating negative values must generate an error",
            1,
            ErrorRecording.testGetNumRecordedErrors(metric, ErrorRecording.ErrorType.InvalidValue))
    }

    @Test
    fun `overflow values accumulate in the last bucket`() {
        // Define a timing distribution metric, which will be stored in "store1".
        val metric = TimingDistributionMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.User,
            name = "test_timing_distribution",
            sendInPings = listOf("store1"),
            timeUnit = TimeUnit.Millisecond
        )

        // Attempt to accumulate an overflow sample
        TimingManager.getElapsedNanos = { 0 }
        val timerId = metric.start()
        TimingManager.getElapsedNanos = { TimingDistributionsStorageEngineImplementation.MAX_SAMPLE_TIME * 2 }
        metric.stopAndAccumulate(timerId)

        // Check that timing distribution was recorded.
        assertTrue("Accumulating overflow values records data",
            metric.testHasValue())

        // Make sure that the overflow landed in the correct (last) bucket
        val hist = FunctionalHistogram(
            TimingDistributionsStorageEngineImplementation.LOG_BASE,
            TimingDistributionsStorageEngineImplementation.BUCKETS_PER_MAGNITUDE
        )
        val snapshot = metric.testGetValue()
        assertEquals("Accumulating overflow values should increment last bucket",
            1L,
            snapshot.values[hist.sampleToBucketMinimum(TimingDistributionsStorageEngineImplementation.MAX_SAMPLE_TIME)])
    }

    @Test
    fun `accumulate finds the correct bucket`() {
        // Define a timing distribution metric, which will be stored in "store1".
        val metric = TimingDistributionMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.User,
            name = "test_timing_distribution",
            sendInPings = listOf("store1"),
            timeUnit = TimeUnit.Millisecond
        )

        val samples = listOf(10L, 100L, 1000L, 10000L)

        // Attempt to accumulate a sample to force metric to be stored
        for (i in samples) {
            TimingManager.getElapsedNanos = { 0 }
            val timerId = metric.start()
            TimingManager.getElapsedNanos = { i }
            metric.stopAndAccumulate(timerId)
        }

        // Check that timing distribution was recorded.
        assertTrue("Accumulating values records data", metric.testHasValue())

        // Make sure that the samples are in the correct buckets
        val snapshot = metric.testGetValue()

        val hist = FunctionalHistogram(
            TimingDistributionsStorageEngineImplementation.LOG_BASE,
            TimingDistributionsStorageEngineImplementation.BUCKETS_PER_MAGNITUDE
        )

        // Check sum and count
        assertEquals("Accumulating updates the sum", 11110, snapshot.sum)
        assertEquals("Accumulating updates the count", 4, snapshot.count)

        for (i in samples) {
            val binEdge = hist.sampleToBucketMinimum(i)
            assertEquals("Accumulating should increment correct bucket", 1L, snapshot.values[binEdge])
        }

        val json = snapshot.toJsonPayloadObject()
        val values = json.getJSONObject("values")
        assertEquals(81, values.length())

        for (i in samples) {
            val binEdge = hist.sampleToBucketMinimum(i)
            assertEquals("Accumulating should increment correct bucket", 1L, values.getLong(binEdge.toString()))
            values.remove(binEdge.toString())
        }

        for (k in values.keys()) {
            assertEquals(0L, values.getLong(k))
        }
    }
}
