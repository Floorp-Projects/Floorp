package mozilla.components.service.glean

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import mozilla.components.service.glean.storages.TimespansStorageEngine
import org.junit.Test

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class TimespanMetricTypeTest {

    @Before
    fun setUp() {
        Glean.initialized = true
        TimespansStorageEngine.applicationContext = ApplicationProvider.getApplicationContext()
        // Clear the stored "user" preferences between tests.
        ApplicationProvider.getApplicationContext<Context>()
            .getSharedPreferences(TimespansStorageEngine.javaClass.simpleName, Context.MODE_PRIVATE)
            .edit()
            .clear()
            .apply()
        TimespansStorageEngine.clearAllStores()
    }

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
        metric.stopAndSum()

        // Check that data was properly recorded.
        val snapshot = TimespansStorageEngine.getSnapshot(storeName = "store1", clearStore = false)
        assertEquals(1, snapshot!!.size)
        assertTrue("telemetry.timespan_metric" in snapshot)
        assertTrue(snapshot["telemetry.timespan_metric"]!! >= 0)
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
        metric.stopAndSum()

        // Let's also call cancel() to make sure it's a no-op.
        metric.cancel()

        // Check that data was not recorded.
        val snapshot = TimespansStorageEngine.getSnapshot(storeName = "store1", clearStore = false)
        assertNull(snapshot)
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
        metric.stopAndSum()

        // Check that data was not recorded.
        val snapshot = TimespansStorageEngine.getSnapshot(storeName = "store1", clearStore = false)
        assertNull(snapshot)
    }
}