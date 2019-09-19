/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package mozilla.components.service.glean.storages

import android.content.Context
import android.content.SharedPreferences
import androidx.test.core.app.ApplicationProvider
import mozilla.components.service.glean.private.DatetimeMetricType
import mozilla.components.service.glean.private.Lifetime
import mozilla.components.service.glean.private.TimeUnit
import mozilla.components.service.glean.testing.GleanTestRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Rule
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

    @get:Rule
    val gleanRule = GleanTestRule(ApplicationProvider.getApplicationContext())

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
        val value = Calendar.getInstance()
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

    @Test
    fun `test that truncation works`() {
        val value = Calendar.getInstance()
        value.set(2004, 11, 9, 8, 3, 29)
        value.set(Calendar.MILLISECOND, 320)
        value.timeZone = TimeZone.getTimeZone("GMT-2")

        val datetimeNanosecondTruncation = DatetimeMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "nanosecond",
            sendInPings = listOf("store1"),
            timeUnit = TimeUnit.Nanosecond
        )
        datetimeNanosecondTruncation.set(value)

        val datetimeMillisecondTruncation = DatetimeMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "millisecond",
            sendInPings = listOf("store1"),
            timeUnit = TimeUnit.Millisecond
        )
        datetimeMillisecondTruncation.set(value)

        val datetimeSecondTruncation = DatetimeMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "second",
            sendInPings = listOf("store1"),
            timeUnit = TimeUnit.Second
        )
        datetimeSecondTruncation.set(value)

        val datetimeMinuteTruncation = DatetimeMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "minute",
            sendInPings = listOf("store1"),
            timeUnit = TimeUnit.Minute
        )
        datetimeMinuteTruncation.set(value)

        val datetimeHourTruncation = DatetimeMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "hour",
            sendInPings = listOf("store1"),
            timeUnit = TimeUnit.Hour
        )
        datetimeHourTruncation.set(value)

        val datetimeDayTruncation = DatetimeMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "day",
            sendInPings = listOf("store1"),
            timeUnit = TimeUnit.Day
        )
        datetimeDayTruncation.set(value)

        // Calendar objects don't have nanosecond resolution, so we don't expect any change,
        // but test anyway so we're testing all of the TimeUnit enumeration values.
        val expectedNanosecond = Calendar.getInstance()
        expectedNanosecond.timeZone = TimeZone.getTimeZone("GMT-2")
        expectedNanosecond.set(2004, 11, 9, 8, 3, 29)
        expectedNanosecond.set(Calendar.MILLISECOND, 320)
        assertEquals(expectedNanosecond.getTime(), datetimeNanosecondTruncation.testGetValue())
        assertEquals("2004-12-09T08:03:29.320-02:00", datetimeNanosecondTruncation.testGetValueAsString())

        val expectedMillisecond = Calendar.getInstance()
        expectedMillisecond.timeZone = TimeZone.getTimeZone("GMT-2")
        expectedMillisecond.set(2004, 11, 9, 8, 3, 29)
        expectedMillisecond.set(Calendar.MILLISECOND, 320)
        assertEquals(expectedMillisecond.getTime(), datetimeMillisecondTruncation.testGetValue())
        assertEquals("2004-12-09T08:03:29.320-02:00", datetimeMillisecondTruncation.testGetValueAsString())

        val expectedSecond = Calendar.getInstance()
        expectedSecond.timeZone = TimeZone.getTimeZone("GMT-2")
        expectedSecond.set(2004, 11, 9, 8, 3, 29)
        expectedSecond.set(Calendar.MILLISECOND, 0)
        assertEquals(expectedSecond.getTime(), datetimeSecondTruncation.testGetValue())
        assertEquals("2004-12-09T08:03:29-02:00", datetimeSecondTruncation.testGetValueAsString())

        val expectedMinute = Calendar.getInstance()
        expectedMinute.timeZone = TimeZone.getTimeZone("GMT-2")
        expectedMinute.set(2004, 11, 9, 8, 3, 0)
        expectedMinute.set(Calendar.MILLISECOND, 0)
        assertEquals(expectedMinute.getTime(), datetimeMinuteTruncation.testGetValue())
        assertEquals("2004-12-09T08:03-02:00", datetimeMinuteTruncation.testGetValueAsString())

        val expectedHour = Calendar.getInstance()
        expectedHour.timeZone = TimeZone.getTimeZone("GMT-2")
        expectedHour.set(2004, 11, 9, 8, 0, 0)
        expectedHour.set(Calendar.MILLISECOND, 0)
        assertEquals(expectedHour.getTime(), datetimeHourTruncation.testGetValue())
        assertEquals("2004-12-09T08-02:00", datetimeHourTruncation.testGetValueAsString())

        val expectedDay = Calendar.getInstance()
        expectedDay.timeZone = TimeZone.getTimeZone("GMT-2")
        expectedDay.set(2004, 11, 9, 0, 0, 0)
        expectedDay.set(Calendar.MILLISECOND, 0)
        assertEquals(expectedDay.getTime(), datetimeDayTruncation.testGetValue())
        assertEquals("2004-12-09-02:00", datetimeDayTruncation.testGetValueAsString())
    }
}
