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
import mozilla.components.service.glean.private.PingType
import mozilla.components.service.glean.resetGlean
import mozilla.components.service.glean.timing.TimingManager
import org.junit.After
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers
import org.mockito.Mockito
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class TimingDistributionsStorageEngineTest {

    @Before
    fun setUp() {
        resetGlean()
    }

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

        // Create a sample that will fall into the first bucket (bucket '0') so we can easily
        // find it
        val sample = 1L
        TimingDistributionsStorageEngine.accumulate(
            metricData = metric,
            sample = sample,
            timeUnit = metric.timeUnit
        )

        // Check that the data was correctly set in each store.
        for (storeName in storeNames) {
            val snapshot = TimingDistributionsStorageEngine.getSnapshot(
                storeName = storeName,
                clearStore = true
            )
            assertEquals(1, snapshot!!.size)
            assertEquals(1L, snapshot["telemetry.test_timing_distribution"]?.values!![0])
        }
    }

    @Test
    fun `deserializer should correctly parse timing distributions`() {
        // We are using the TimingDistributionData here as a way to turn the object
        // into JSON for easy comparison
        val metric = TimingDistributionMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "test_timing_distribution",
            sendInPings = listOf("store1", "store2"),
            timeUnit = TimeUnit.Millisecond
        )
        val td = TimingDistributionData(category = metric.category, name = metric.name)

        val persistedSample = mapOf(
            "store1#telemetry.invalid_string" to "invalid_string",
            "store1#telemetry.invalid_bool" to false,
            "store1#telemetry.null" to null,
            "store1#telemetry.invalid_int" to -1,
            "store1#telemetry.invalid_list" to listOf("1", "2", "3"),
            "store1#telemetry.invalid_int_list" to "[1,2,3]",
            "store1#telemetry.invalid_td_name" to "{\"category\":\"telemetry\",\"bucket_count\":100,\"range\":[0,60000,12],\"histogram_type\":1,\"values\":{},\"sum\":0,\"time_unit\":2}",
            "store1#telemetry.invalid_td_bucket_count" to "{\"category\":\"telemetry\",\"name\":\"test_timing_distribution\",\"bucket_count\":\"not an int!\",\"range\":[0,60000,12],\"histogram_type\":1,\"values\":{},\"sum\":0,\"time_unit\":2}",
            "store1#telemetry.invalid_td_range" to "{\"category\":\"telemetry\",\"name\":\"test_timing_distribution\",\"bucket_count\":100,\"range\":[0,60000,12],\"histogram_type\":1,\"values\":{},\"sum\":0,\"time_unit\":2}",
            "store1#telemetry.invalid_td_range2" to "{\"category\":\"telemetry\",\"name\":\"test_timing_distribution\",\"bucket_count\":100,\"range\":[\"not\",\"numeric\"],\"histogram_type\":1,\"values\":{},\"sum\":0,\"time_unit\":2}",
            "store1#telemetry.invalid_td_histogram_type" to "{\"category\":\"telemetry\",\"name\":\"test_timing_distribution\",\"bucket_count\":100,\"range\":[0,60000,12],\"histogram_type\":-1,\"values\":{},\"sum\":0,\"time_unit\":2}",
            "store1#telemetry.invalid_td_values" to "{\"category\":\"telemetry\",\"name\":\"test_timing_distribution\",\"bucket_count\":100,\"range\":[0,60000,12],\"histogram_type\":1,\"values\":{\"0\": \"nope\"},\"sum\":0,\"time_unit\":2}",
            "store1#telemetry.invalid_td_sum" to "{\"category\":\"telemetry\",\"name\":\"test_timing_distribution\",\"bucket_count\":100,\"range\":[0,60000,12],\"histogram_type\":1,\"values\":{},\"sum\":\"nope\",\"time_unit\":2}",
            "store1#telemetry.invalid_td_time_unit" to "{\"category\":\"telemetry\",\"name\":\"test_timing_distribution\",\"bucket_count\":100,\"range\":[0,60000,12],\"histogram_type\":1,\"values\":{},\"sum\":0,\"time_unit\":-1}",
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

            // Using the TimingDistributionData object here to easily turn the object into JSON
            // for comparison purposes.
            val td = TimingDistributionData(category = metric.category, name = metric.name)
            td.accumulate(1L)

            runBlocking {
                storageEngine.accumulate(
                    metricData = metric,
                    sample = 1000000L,
                    timeUnit = metric.timeUnit
                )
            }

            // Get snapshot from store1
            val json = storageEngine.getSnapshotAsJSON("store1", true)
            // Check for correct JSON serialization
            assertEquals("{\"${metric.identifier}\":${td.toJsonObject()}}",
                json.toString()
            )
        }

        // Create a new instance of storage engine to verify serialization to storage rather than
        // to the cache
        run {
            val storageEngine = TimingDistributionsStorageEngineImplementation()
            storageEngine.applicationContext = ApplicationProvider.getApplicationContext()

            val td = TimingDistributionData(category = "telemetry", name = "test_timing_distribution")
            td.accumulate(1L)

            // Get snapshot from store1
            val json = storageEngine.getSnapshotAsJSON("store1", true)
            // Check for correct JSON serialization
            assertEquals("{\"telemetry.test_timing_distribution\":${td.toJsonObject()}}",
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
        TimingManager.getElapsedNanos = { (TimingDistributionData.DEFAULT_RANGE_MAX + 100) * 1000000 }
        metric.stopAndAccumulate(timerId)

        // Check that timing distribution was recorded.
        assertTrue("Accumulating overflow values records data",
            metric.testHasValue())

        // Make sure that the overflow landed in the correct (last) bucket
        val snapshot = metric.testGetValue()
        assertEquals("Accumulating overflow values should increment last bucket",
            1L,
            snapshot.values[TimingDistributionData.DEFAULT_BUCKET_COUNT - 1])
    }

    @Test
    fun `getBuckets() correctly populates the buckets property`() {
        // Hand calculated values using current default range 0 - 60000 and bucket count of 100.
        // NOTE: The final bucket, regardless of width, represents the overflow bucket to hold any
        // values beyond the maximum (in this case the maximum is 60000)
        val testBuckets: List<Long> = listOf(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 17,
            19, 21, 23, 25, 28, 31, 34, 38, 42, 46, 51, 56, 62, 68, 75, 83, 92, 101, 111, 122, 135,
            149, 164, 181, 200, 221, 244, 269, 297, 328, 362, 399, 440, 485, 535, 590, 651, 718,
            792, 874, 964, 1064, 1174, 1295, 1429, 1577, 1740, 1920, 2118, 2337, 2579, 2846, 3140,
            3464, 3822, 4217, 4653, 5134, 5665, 6250, 6896, 7609, 8395, 9262, 10219, 11275, 12440,
            13726, 15144, 16709, 18436, 20341, 22443, 24762, 27321, 30144, 33259, 36696, 40488,
            44672, 49288, 54381, 60000, 60001)

        // Define a timing distribution metric, which will be stored in "store1".
        val metric = TimingDistributionMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.User,
            name = "test_timing_distribution",
            sendInPings = listOf("store1"),
            timeUnit = TimeUnit.Millisecond
        )

        // Accumulate a sample to force the lazy loading of `buckets` to occur
        TimingManager.getElapsedNanos = { 0 }
        val timerId = metric.start()
        TimingManager.getElapsedNanos = { 1 }
        metric.stopAndAccumulate(timerId)

        // Check that timing distribution was recorded.
        assertTrue("Accumulating values records data", metric.testHasValue())

        // Make sure that the sample in the correct (first) bucket
        val snapshot = metric.testGetValue()
        assertEquals("Accumulating should increment correct bucket",
            1L, snapshot.values[0])

        // verify buckets lists worked
        assertNotNull("Buckets must not be null", snapshot.buckets)

        assertEquals("Bucket calculation failed", testBuckets, snapshot.buckets)
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

        // Check that a few values correctly fall into the correct buckets (as calculated by hand)
        // to validate the linear bucket search algorithm

        // Attempt to accumulate a sample to force metric to be stored
        for (i in listOf(1L, 10L, 100L, 1000L, 10000L)) {
            TimingManager.getElapsedNanos = { 0 }
            val timerId = metric.start()
            TimingManager.getElapsedNanos = { i * 1000000 } // Convert ms to ns
            metric.stopAndAccumulate(timerId)
        }

        // Check that timing distribution was recorded.
        assertTrue("Accumulating values records data", metric.testHasValue())

        // Make sure that the samples are in the correct buckets
        val snapshot = metric.testGetValue()

        // Check sum and count
        assertEquals("Accumulating updates the sum", 11111, snapshot.sum)
        assertEquals("Accumulating updates the count", 5, snapshot.count)

        assertEquals("Accumulating should increment correct bucket",
            1L, snapshot.values[0])
        assertEquals("Accumulating should increment correct bucket",
            1L, snapshot.values[9])
        assertEquals("Accumulating should increment correct bucket",
            1L, snapshot.values[33])
        assertEquals("Accumulating should increment correct bucket",
            1L, snapshot.values[57])
        assertEquals("Accumulating should increment correct bucket",
            1L, snapshot.values[80])
    }

    @Test
    fun `toJsonObject correctly converts a TimingDistributionData object`() {
        // Define a TimingDistributionData object
        val tdd = TimingDistributionData(category = "telemetry", name = "test_timing_distribution")

        // Accumulate some samples to populate sum and values properties
        tdd.accumulate(1L)
        tdd.accumulate(2L)
        tdd.accumulate(3L)

        // Convert to JSON object using toJsonObject()
        val jsonTdd = tdd.toJsonObject()

        // Verify properties
        assertEquals("JSON category must match Timing Distribution category",
            "telemetry", jsonTdd.getString("category"))
        assertEquals("JSON name must match Timing Distribution name",
            "test_timing_distribution", jsonTdd.getString("name"))
        assertEquals("JSON bucket count must match Timing Distribution bucket count",
            tdd.bucketCount, jsonTdd.getInt("bucket_count"))
        assertEquals("JSON name must match Timing Distribution name",
            "test_timing_distribution", jsonTdd.getString("name"))
        val jsonRange = jsonTdd.getJSONArray("range")
        assertEquals("JSON range minimum must match Timing Distribution range minimum",
            tdd.rangeMin, jsonRange.getLong(0))
        assertEquals("JSON range maximum must match Timing Distribution range maximum",
            tdd.rangeMax, jsonRange.getLong(1))
        assertEquals("JSON histogram type must match Timing Distribution histogram type",
            tdd.histogramType.toString().toLowerCase(), jsonTdd.getString("histogram_type"))
        val jsonValue = jsonTdd.getJSONObject("values")
        assertEquals("JSON values must match Timing Distribution values",
            tdd.values[0], jsonValue.getLong("0"))
        assertEquals("JSON values must match Timing Distribution values",
            tdd.values[1], jsonValue.getLong("1"))
        assertEquals("JSON values must match Timing Distribution values",
            tdd.values[2], jsonValue.getLong("2"))
        assertEquals("JSON sum must match Timing Distribution sum",
            tdd.sum, jsonTdd.getLong("sum"))
        assertEquals("JSON time unit must match Timing Distribution time unit",
            tdd.timeUnit.toString().toLowerCase(), jsonTdd.getString("time_unit"))
    }
}
