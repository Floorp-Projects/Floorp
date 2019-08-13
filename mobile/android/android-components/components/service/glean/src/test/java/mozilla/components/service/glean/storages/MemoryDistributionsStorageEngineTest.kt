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
import mozilla.components.service.glean.private.MemoryDistributionMetricType
import mozilla.components.service.glean.error.ErrorRecording
import mozilla.components.service.glean.histogram.FunctionalHistogram
import mozilla.components.service.glean.private.MemoryUnit
import mozilla.components.service.glean.private.PingType
import mozilla.components.service.glean.testing.GleanTestRule
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
class MemoryDistributionsStorageEngineTest {

    @get:Rule
    val gleanRule = GleanTestRule(ApplicationProvider.getApplicationContext())

    @Test
    fun `accumulate() properly updates the values in all stores`() {
        val storeNames = listOf("store1", "store2")

        val metric = MemoryDistributionMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "test_memory_distribution",
            sendInPings = storeNames,
            memoryUnit = MemoryUnit.Kilobyte
        )

        // Create a sample that will fall into the underflow bucket (bucket '0') so we can easily
        // find it
        val sample = 1L
        MemoryDistributionsStorageEngine.accumulate(
            metricData = metric,
            sample = sample,
            memoryUnit = MemoryUnit.Kilobyte
        )

        // Check that the data was correctly set in each store.
        for (storeName in storeNames) {
            val snapshot = MemoryDistributionsStorageEngine.getSnapshot(
                storeName = storeName,
                clearStore = true
            )
            assertEquals(1, snapshot!!.size)
            assertEquals(1L, snapshot["telemetry.test_memory_distribution"]?.values!![1023])
        }
    }

    @Test
    fun `deserializer should correctly parse memory distributions`() {
        val md = FunctionalHistogram(
            MemoryDistributionsStorageEngineImplementation.LOG_BASE,
            MemoryDistributionsStorageEngineImplementation.BUCKETS_PER_MAGNITUDE
        )

        val persistedSample = mapOf(
            "store1#telemetry.invalid_string" to "invalid_string",
            "store1#telemetry.invalid_bool" to false,
            "store1#telemetry.null" to null,
            "store1#telemetry.invalid_int" to -1,
            "store1#telemetry.invalid_list" to listOf("1", "2", "3"),
            "store1#telemetry.invalid_int_list" to "[1,2,3]",
            "store1#telemetry.invalid_md_values" to "{\"log_base\":2.0,\"buckets_per_magnitude\":16.0,\"values\":{\"0\": \"nope\"},\"sum\":0}",
            "store1#telemetry.invalid_md_sum" to "{\"log_base\":2.0,\"buckets_per_magnitude\":16.0,\"values\":{},\"sum\":\"nope\"}",
            "store1#telemetry.test_memory_distribution" to md.toJsonObject().toString()
        )

        val storageEngine = MemoryDistributionsStorageEngineImplementation()

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
        assertEquals(md.toJsonObject().toString(),
            snapshot["telemetry.test_memory_distribution"]?.toJsonObject().toString())
    }

    @Test
    fun `serializer should serialize memory distribution that matches schema`() {
        val ping1 = PingType("store1", true)

        val metric = MemoryDistributionMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.User,
            name = "test_memory_distribution",
            sendInPings = listOf("store1"),
            memoryUnit = MemoryUnit.Kilobyte
        )

        runBlocking {
            metric.accumulate(100000L)
        }

        collectAndCheckPingSchema(ping1)
    }

    @Test
    fun `serializer should correctly serialize memory distributions`() {
        run {
            val storageEngine = MemoryDistributionsStorageEngineImplementation()
            storageEngine.applicationContext = ApplicationProvider.getApplicationContext()

            val metric = MemoryDistributionMetricType(
                disabled = false,
                category = "telemetry",
                lifetime = Lifetime.User,
                name = "test_memory_distribution",
                sendInPings = listOf("store1", "store2"),
                memoryUnit = MemoryUnit.Kilobyte
            )

            // Using the FunctionalHistogram object here to easily turn the object into JSON
            // for comparison purposes.
            val md = FunctionalHistogram(
                MemoryDistributionsStorageEngineImplementation.LOG_BASE,
                MemoryDistributionsStorageEngineImplementation.BUCKETS_PER_MAGNITUDE
            )
            md.accumulate(1000000L * 1024L)

            runBlocking {
                storageEngine.accumulate(
                    metricData = metric,
                    sample = 1000000L,
                    memoryUnit = MemoryUnit.Kilobyte
                )
            }

            // Get snapshot from store1
            val json = storageEngine.getSnapshotAsJSON("store1", true)
            // Check for correct JSON serialization
            assertEquals("{\"${metric.identifier}\":${md.toJsonPayloadObject()}}",
                json.toString()
            )
        }

        // Create a new instance of storage engine to verify serialization to storage rather than
        // to the cache
        run {
            val storageEngine = MemoryDistributionsStorageEngineImplementation()
            storageEngine.applicationContext = ApplicationProvider.getApplicationContext()

            val md = FunctionalHistogram(
                MemoryDistributionsStorageEngineImplementation.LOG_BASE,
                MemoryDistributionsStorageEngineImplementation.BUCKETS_PER_MAGNITUDE
            )
            md.accumulate(1000000L * 1024)

            // Get snapshot from store1
            val json = storageEngine.getSnapshotAsJSON("store1", true)
            // Check for correct JSON serialization
            assertEquals("{\"telemetry.test_memory_distribution\":${md.toJsonPayloadObject()}}",
                json.toString()
            )
        }
    }

    @Test
    fun `memory distributions must not accumulate negative values`() {
        // Define a memory distribution metric, which will be stored in "store1".
        val metric = MemoryDistributionMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.User,
            name = "test_memory_distribution",
            sendInPings = listOf("store1"),
            memoryUnit = MemoryUnit.Byte
        )

        metric.accumulate(-1)
        // Check that nothing was recorded.
        assertFalse("Memory distributions must not accumulate negative values",
            metric.testHasValue())

        // Make sure that the errors have been recorded
        assertEquals("Accumulating negative values must generate an error",
            1,
            ErrorRecording.testGetNumRecordedErrors(metric, ErrorRecording.ErrorType.InvalidValue))
    }

    @Test
    fun `overflow values accumulate in the last bucket`() {
        // Define a memory distribution metric, which will be stored in "store1".
        val metric = MemoryDistributionMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.User,
            name = "test_memory_distribution",
            sendInPings = listOf("store1"),
            memoryUnit = MemoryUnit.Byte
        )

        // Attempt to accumulate an overflow sample
        metric.accumulate(1L shl 41)

        val hist = FunctionalHistogram(
            MemoryDistributionsStorageEngineImplementation.LOG_BASE,
            MemoryDistributionsStorageEngineImplementation.BUCKETS_PER_MAGNITUDE
        )

        // Check that memory distribution was recorded.
        assertTrue("Accumulating overflow values records data",
            metric.testHasValue())

        // Make sure that the overflow landed in the correct (last) bucket
        val snapshot = metric.testGetValue()
        assertEquals("Accumulating overflow values should increment last bucket",
            1L,
            snapshot.values[hist.sampleToBucketMinimum(MemoryDistributionsStorageEngineImplementation.MAX_BYTES)])
    }

    @Test
    fun `accumulate finds the correct bucket`() {
        // Define a memory distribution metric, which will be stored in "store1".
        val metric = MemoryDistributionMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.User,
            name = "test_memory_distribution",
            sendInPings = listOf("store1"),
            memoryUnit = MemoryUnit.Byte
        )

        val samples = listOf(10L, 100L, 1000L, 10000L)

        // Attempt to accumulate a sample to force metric to be stored
        for (i in samples) {
            metric.accumulate(i)
        }

        // Check that memory distribution was recorded.
        assertTrue("Accumulating values records data", metric.testHasValue())

        // Make sure that the samples are in the correct buckets
        val snapshot = metric.testGetValue()

        // Check sum and count
        assertEquals("Accumulating updates the sum", 11110, snapshot.sum)
        assertEquals("Accumulating updates the count", 4, snapshot.count)

        val hist = FunctionalHistogram(
            MemoryDistributionsStorageEngineImplementation.LOG_BASE,
            MemoryDistributionsStorageEngineImplementation.BUCKETS_PER_MAGNITUDE
        )

        for (i in samples) {
            val binEdge = hist.sampleToBucketMinimum(i)
            assertEquals("Accumulating should increment correct bucket", 1L, snapshot.values[binEdge])
        }

        val json = snapshot.toJsonPayloadObject()
        val values = json.getJSONObject("values")
        assertEquals(154, values.length())

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
