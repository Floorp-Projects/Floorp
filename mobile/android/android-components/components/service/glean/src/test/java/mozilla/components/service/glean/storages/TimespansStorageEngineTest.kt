/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package mozilla.components.service.glean.storages

import android.content.Context
import android.content.SharedPreferences
import androidx.test.core.app.ApplicationProvider
import mozilla.components.service.glean.metrics.CommonMetricData
import mozilla.components.service.glean.metrics.Lifetime
import mozilla.components.service.glean.metrics.TimeUnit

import org.junit.Before
import org.junit.Test
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.runner.RunWith
import org.mockito.Mockito.doReturn
import org.mockito.Mockito.eq
import org.mockito.Mockito.mock
import org.mockito.Mockito.spy
import org.mockito.Mockito.`when`
import org.robolectric.RobolectricTestRunner
import java.util.concurrent.TimeUnit as AndroidTimeUnit

@RunWith(RobolectricTestRunner::class)
class TimespansStorageEngineTest {

    private data class TestMetricData(
        override val disabled: Boolean,
        override val category: String,
        override val lifetime: Lifetime,
        override val name: String,
        override val sendInPings: List<String>,
        override val defaultStorageDestinations: List<String> = listOf()
    ) : CommonMetricData

    /**
     * Helper that returns a spied mocked engine.
     */
    private fun getSpiedEngine(): TimespansStorageEngineImplementation {
        // We need to initialize `engine` and `spiedEngine` separately, otherwise
        // it will throw a RuntimeException because of the `applicationContext` not being
        // initialized.
        val engine = TimespansStorageEngineImplementation()
        engine.applicationContext = ApplicationProvider.getApplicationContext()
        return spy<TimespansStorageEngineImplementation>(engine)
    }

    @Before
    fun setUp() {
        TimespansStorageEngine.applicationContext = ApplicationProvider.getApplicationContext()
        TimespansStorageEngine.clearAllStores()
    }

    @Test
    fun `timespan deserializer should correctly parse JSONArray(s)`() {
        val expectedValue: Long = 37
        val persistedSample = mapOf(
            "store1#telemetry.invalid_bool" to false,
            "store1#telemetry.invalid_string" to "c4ff33",
            "store1#telemetry.valid" to "[${TimeUnit.Nanosecond.ordinal}, $expectedValue]"
        )

        val storageEngine = TimespansStorageEngineImplementation()

        // Create a fake application context that will be used to load our data.
        val context = mock(Context::class.java)
        val sharedPreferences = mock(SharedPreferences::class.java)
        `when`(sharedPreferences.all).thenAnswer { persistedSample }
        `when`(context.getSharedPreferences(
            eq(storageEngine::class.java.canonicalName),
            eq(Context.MODE_PRIVATE)
        )).thenReturn(sharedPreferences)
        `when`(context.getSharedPreferences(
            eq("${storageEngine::class.java.canonicalName}.PingLifetime"),
            eq(Context.MODE_PRIVATE)
        )).thenReturn(ApplicationProvider.getApplicationContext<Context>()
            .getSharedPreferences("${storageEngine::class.java.canonicalName}.PingLifetime",
                Context.MODE_PRIVATE))

        storageEngine.applicationContext = context
        val snapshot = storageEngine.getSnapshotWithTimeUnit(storeName = "store1", clearStore = true)
        assertEquals(1, snapshot!!.size)
        assertEquals(Pair("nanosecond", expectedValue), snapshot["telemetry.valid"])
    }

    @Test
    fun `a single elapsed time must be correctly recorded`() {
        val expectedTimespanNanos: Long = 37

        val metric = TestMetricData(
            false,
            "telemetry",
            Lifetime.Ping,
            "single_elapsed_test",
            listOf("store1"))

        // Initialize a mocked engine so that we can return a custom elapsed time.
        val spiedEngine = getSpiedEngine()

        // Return 0 the first time we get the time.
        doReturn(0.toLong()).`when`(spiedEngine).getElapsedNanos()
        spiedEngine.start(metric)

        // Return the expected time when we stop the timer.
        doReturn(expectedTimespanNanos).`when`(spiedEngine).getElapsedNanos()
        spiedEngine.stopAndSum(metric, TimeUnit.Nanosecond)

        val snapshot = spiedEngine.getSnapshotWithTimeUnit(storeName = "store1", clearStore = false)
        assertEquals(1, snapshot!!.size)
        assertEquals(Pair("nanosecond", expectedTimespanNanos), snapshot["telemetry.single_elapsed_test"])
    }

    @Test
    fun `multiple elapsed times must be correctly accumulated`() {
        val expectedChunkNanos: Long = 37

        val metric = TestMetricData(
            false,
            "telemetry",
            Lifetime.Ping,
            "single_elapsed_test",
            listOf("store1"))

        // Initialize a mocked engine so that we can return a custom elapsed time.
        val spiedEngine = getSpiedEngine()

        // Record the time for the first chunk.
        doReturn(0.toLong()).`when`(spiedEngine).getElapsedNanos()
        spiedEngine.start(metric)
        doReturn(expectedChunkNanos).`when`(spiedEngine).getElapsedNanos()
        spiedEngine.stopAndSum(metric, TimeUnit.Nanosecond)

        // Record the time for the second chunk of time.
        spiedEngine.start(metric)
        doReturn(expectedChunkNanos * 2).`when`(spiedEngine).getElapsedNanos()
        spiedEngine.stopAndSum(metric, TimeUnit.Nanosecond)

        val snapshot = spiedEngine.getSnapshotWithTimeUnit(storeName = "store1", clearStore = false)
        assertEquals(1, snapshot!!.size)
        assertEquals(Pair("nanosecond", expectedChunkNanos * 2), snapshot["telemetry.single_elapsed_test"])
    }

    @Test
    fun `the API must not crash nor throw errors when called in unexpected ways`() {
        val metric = TestMetricData(
            false,
            "telemetry",
            Lifetime.Ping,
            "single_elapsed_test",
            listOf("store1"))

        val spiedEngine = getSpiedEngine()

        // Check that a call to cancel() without any previous start doesn't fail.
        spiedEngine.cancel(metric)

        // Same for stopAndSum().
        spiedEngine.stopAndSum(metric, TimeUnit.Nanosecond)
    }

    @Test
    fun `start() must preserve the original time if called twice for the same metric`() {
        val expectedChunkNanos: Long = 37

        val metric = TestMetricData(
            false,
            "telemetry",
            Lifetime.Ping,
            "single_elapsed_test",
            listOf("store1"))

        val spiedEngine = getSpiedEngine()

        // Call start() the first time.
        doReturn(0.toLong()).`when`(spiedEngine).getElapsedNanos()
        spiedEngine.start(metric)

        // Change the elapsed time and call start the second time.
        doReturn(10.toLong()).`when`(spiedEngine).getElapsedNanos()
        spiedEngine.start(metric)

        // Call stopAndSum and make sure the elapsed time is still referred to the first
        // start() call.
        doReturn(expectedChunkNanos).`when`(spiedEngine).getElapsedNanos()
        spiedEngine.stopAndSum(metric, TimeUnit.Nanosecond)

        val snapshot = spiedEngine.getSnapshotWithTimeUnit(storeName = "store1", clearStore = false)
        assertEquals(1, snapshot!!.size)
        assertEquals(
            Pair("nanosecond", expectedChunkNanos),
            snapshot["telemetry.single_elapsed_test"]
        )
    }

    @Test
    fun `calling cancel() after start() should clear the uncomitted elapsed time`() {
        val metric = TestMetricData(
            false,
            "telemetry",
            Lifetime.Ping,
            "single_elapsed_test",
            listOf("store1"))

        val spiedEngine = getSpiedEngine()

        // Call start() the first time.
        doReturn(10.toLong()).`when`(spiedEngine).getElapsedNanos()
        spiedEngine.start(metric)

        // Cancel the current timespan.
        spiedEngine.cancel(metric)

        // Call stopAndSum: since the time was cleared we don't expected anything in the
        // snapshot.
        doReturn(20.toLong()).`when`(spiedEngine).getElapsedNanos()
        spiedEngine.stopAndSum(metric, TimeUnit.Nanosecond)

        val snapshot = spiedEngine.getSnapshotWithTimeUnit(storeName = "store1", clearStore = false)
        assertNull(snapshot)
    }

    @Test
    fun `the recorded time must conform to the chosen resolution`() {
        val expectedLengthInNanos: Long = AndroidTimeUnit.DAYS.toNanos(3)
        val expectedResults = mapOf(
            TimeUnit.Nanosecond to expectedLengthInNanos,
            TimeUnit.Microsecond to AndroidTimeUnit.NANOSECONDS.toMicros(expectedLengthInNanos),
            TimeUnit.Millisecond to AndroidTimeUnit.NANOSECONDS.toMillis(expectedLengthInNanos),
            TimeUnit.Second to AndroidTimeUnit.NANOSECONDS.toSeconds(expectedLengthInNanos),
            TimeUnit.Minute to AndroidTimeUnit.NANOSECONDS.toMinutes(expectedLengthInNanos),
            TimeUnit.Hour to AndroidTimeUnit.NANOSECONDS.toHours(expectedLengthInNanos),
            TimeUnit.Day to AndroidTimeUnit.NANOSECONDS.toDays(expectedLengthInNanos)
        )

        val metric = TestMetricData(
            false,
            "telemetry",
            Lifetime.Ping,
            "resolution_test",
            listOf("store1"))

        val spiedEngine = getSpiedEngine()

        expectedResults.forEach { (res, expectedTimespan) ->
            // Record the timespan in the provided resolution.
            doReturn(0.toLong()).`when`(spiedEngine).getElapsedNanos()
            spiedEngine.start(metric)
            doReturn(expectedLengthInNanos).`when`(spiedEngine).getElapsedNanos()
            spiedEngine.stopAndSum(metric, res)

            val snapshot = spiedEngine.getSnapshotWithTimeUnit(storeName = "store1", clearStore = true)
            assertEquals(1, snapshot!!.size)
            assertEquals(Pair(res.name.toLowerCase(), expectedTimespan), snapshot["telemetry.resolution_test"])
        }
    }

    @Test
    fun `accumulated short-lived timespans should not be discarded`() {
        // We'll attempt to accumulate 10 timespans with a sub-second duration.
        val steps = 10
        val expectedTimespanSeconds: Long = 1
        val timespanFraction: Long = AndroidTimeUnit.SECONDS.toNanos(expectedTimespanSeconds) / steps

        val metric = TestMetricData(
            false,
            "telemetry",
            Lifetime.Ping,
            "many_short_lived_test",
            listOf("store1"))

        val spiedEngine = getSpiedEngine()

        // Accumulate enough timespans with a duration lower than the
        // expected resolution. Their sum should still be >= than our
        // resolution, and thus be reported.
        for (i in 0..steps) {
            val timespanStart: Long = i * timespanFraction
            doReturn(timespanStart).`when`(spiedEngine).getElapsedNanos()
            spiedEngine.start(metric)

            val timespanEnd: Long = timespanStart + timespanFraction
            doReturn(timespanEnd).`when`(spiedEngine).getElapsedNanos()
            spiedEngine.stopAndSum(metric, TimeUnit.Second)
        }

        // Since the sum of the short-lived timespans is >= our resolution, we
        // expect the accumulated value to be in the snapshot.
        val snapshot = spiedEngine.getSnapshotWithTimeUnit(storeName = "store1", clearStore = true)
        assertEquals(1, snapshot!!.size)
        assertEquals(
            Pair("second", expectedTimespanSeconds),
            snapshot["telemetry.many_short_lived_test"]
        )
    }
}
