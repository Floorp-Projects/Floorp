/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package mozilla.components.service.glean.storages

import android.content.Context
import android.content.SharedPreferences
import androidx.test.core.app.ApplicationProvider
import mozilla.components.service.glean.Lifetime
import mozilla.components.service.glean.DatetimeMetricType
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.ArgumentMatchers.eq
import org.mockito.Mockito.`when`
import org.mockito.Mockito.mock
import org.robolectric.RobolectricTestRunner
import java.util.Calendar
import java.util.TimeZone

@RunWith(RobolectricTestRunner::class)
class DatetimesStorageEngineTest {

    @Before
    fun setUp() {
        DatetimesStorageEngine.applicationContext = ApplicationProvider.getApplicationContext()
        // Clear the stored "user" preferences between tests.
        ApplicationProvider.getApplicationContext<Context>()
            .getSharedPreferences(DatetimesStorageEngine.javaClass.simpleName, Context.MODE_PRIVATE)
            .edit()
            .clear()
            .apply()
        DatetimesStorageEngine.clearAllStores()
    }

    @Test
    fun `datetime deserializer should correctly parse datetimes`() {
        val persistedSample = mapOf(
            "store1#telemetry.invalid_date" to "2019-13-01T00:00:00Z",
            "store1#telemetry.null" to null,
            "store1#telemetry.invalid_string" to "test",
            "store1#telemetry.valid_date1" to "1993-02-23T09:05:23.320-08:00",
            "store1#telemetry.valid_date2" to "1993-02-23T09:05:23-08:00",
            "store1#telemetry.valid_date3" to "1993-02-23T09:05-08:00",
            "store1#telemetry.valid_date4" to "1993-02-23T09-08:00",
            "store1#telemetry.valid_date5" to "1993-02-23-08:00"
        )

        val storageEngine = DatetimesStorageEngineImplementation()

        // Create a fake application context that will be used to load our data.
        val context = mock(Context::class.java)
        val sharedPreferences = mock(SharedPreferences::class.java)
        `when`(sharedPreferences.all).thenAnswer { persistedSample }
        `when`(context.getSharedPreferences(
            eq(storageEngine::class.java.simpleName),
            eq(Context.MODE_PRIVATE)
        )).thenReturn(sharedPreferences)

        storageEngine.applicationContext = context
        val snapshot = storageEngine.getSnapshot(storeName = "store1", clearStore = true)
        assertEquals(5, snapshot!!.size)
        assertEquals("1993-02-23T09:05:23.320-08:00", snapshot["telemetry.valid_date1"])
        assertEquals("1993-02-23T09:05:23-08:00", snapshot["telemetry.valid_date2"])
        assertEquals("1993-02-23T09:05-08:00", snapshot["telemetry.valid_date3"])
        assertEquals("1993-02-23T09-08:00", snapshot["telemetry.valid_date4"])
        assertEquals("1993-02-23-08:00", snapshot["telemetry.valid_date5"])
    }

    @Test
    fun `datetime serializer should correctly serialize datetimes`() {
        val value = Calendar.getInstance()!!
        value.set(1993, 1, 23, 9, 5, 23)
        value.set(Calendar.MILLISECOND, 0)
        value.setTimeZone(TimeZone.getTimeZone("America/Los_Angeles"))

        run {
            val storageEngine = DatetimesStorageEngineImplementation()
            storageEngine.applicationContext = ApplicationProvider.getApplicationContext()

            val metric = DatetimeMetricType(
                disabled = false,
                category = "telemetry",
                lifetime = Lifetime.User,
                name = "datetime_metric",
                sendInPings = listOf("store1")
            )

            // Record the datetime in the store, without providing optional arguments.
            storageEngine.set(metric, value)

            // Get the snapshot from "store1"
            val snapshot = storageEngine.getSnapshotAsJSON(storeName = "store1",
                clearStore = true)
            // Check that this serializes to the expected JSON format.
            assertEquals("{\"telemetry.datetime_metric\":\"1993-02-23T09:05-08:00\"}",
                snapshot.toString())
        }

        // Create a new instance of storage engine to verify serialization to storage rather than
        // to the cache
        run {
            val storageEngine = DatetimesStorageEngineImplementation()
            storageEngine.applicationContext = ApplicationProvider.getApplicationContext()

            // Get the snapshot from "store1"
            val snapshot = storageEngine.getSnapshotAsJSON(storeName = "store1",
                clearStore = true)
            // Check that this serializes to the expected JSON format.
            assertEquals("{\"telemetry.datetime_metric\":\"1993-02-23T09:05-08:00\"}",
                snapshot.toString())
        }
    }

    @Test
    fun `setValue() properly sets the value in all stores`() {
        val storeNames = listOf("store1", "store2")

        val metric = DatetimeMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "datetime_metric",
            sendInPings = storeNames
        )

        // Record the datetime in the stores, without providing optional arguments.
        DatetimesStorageEngine.set(metric)

        // Check that the data was correctly set in each store.
        for (storeName in storeNames) {
            metric.testHasValue(storeName)
        }
    }

    @Test
    fun `getSnapshot() returns null if nothing is recorded in the store`() {
        assertNull("The engine must report 'null' on empty or unknown stores",
            DatetimesStorageEngine.getSnapshot(storeName = "unknownStore", clearStore = false))
    }

    @Test
    fun `getSnapshot() correctly clears the stores`() {
        val storeNames = listOf("store1", "store2")

        val metric = DatetimeMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "datetime_metric",
            sendInPings = storeNames
        )

        // Record the datetime in the stores, without providing optional arguments.
        DatetimesStorageEngine.set(metric)

        // Get the snapshot from "store1" and clear it.
        val snapshot = DatetimesStorageEngine.getSnapshot(storeName = "store1", clearStore = true)
        // Check that getting a new snapshot for "store1" returns an empty store.
        assertNull("The engine must report 'null' on empty stores",
            DatetimesStorageEngine.getSnapshot(storeName = "store1", clearStore = false))

        // Check that we get the right data from both the stores. Clearing "store1" must
        // not clear "store2" as well.
        val snapshot2 = DatetimesStorageEngine.getSnapshot(storeName = "store2", clearStore = false)
        for (s in listOf(snapshot, snapshot2)) {
            assertEquals(1, s!!.size)
        }
    }
}
