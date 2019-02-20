/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.ObsoleteCoroutinesApi
import mozilla.components.service.glean.storages.DatetimesStorageEngine
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import java.util.Calendar
import java.util.TimeZone

@ObsoleteCoroutinesApi
@ExperimentalCoroutinesApi
@RunWith(RobolectricTestRunner::class)
class DatetimeMetricTypeTest {

    @get:Rule
    val fakeDispatchers = FakeDispatchersInTest()

    @Before
    fun setUp() {
        Glean.initialized = true
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
    fun `The API must define the expected "default" storage`() {
        // Define a 'datetimeMetric' datetime metric, which will be stored in "store1"
        val datetimeMetric = DatetimeMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "datetime_metric",
            sendInPings = listOf("store1")
        )
        assertEquals(listOf("metrics"), datetimeMetric.defaultStorageDestinations)
    }

    @Test
    fun `The API saves to its storage engine`() {
        // Define a 'datetimeMetric' datetime metric, which will be stored in "store1"
        val datetimeMetric = DatetimeMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "datetime_metric",
            sendInPings = listOf("store1")
        )

        val value = Calendar.getInstance()!!
        value.set(2004, 11, 9, 8, 3, 29)
        value.timeZone = TimeZone.getTimeZone("America/Los_Angeles")
        datetimeMetric.set(value)
        assertTrue(datetimeMetric.testHasValue())
        assertEquals("2004-12-09T08:03-08:00", datetimeMetric.testGetValueAsString())

        val value2 = Calendar.getInstance()!!
        value2.set(1993, 1, 23, 9, 5, 43)
        value2.timeZone = TimeZone.getTimeZone("GMT+0")
        datetimeMetric.set(value2)
        // Check that data was properly recorded.
        assertTrue(datetimeMetric.testHasValue())
        assertEquals("1993-02-23T09:05+00:00", datetimeMetric.testGetValueAsString())

        // A date prior to the UNIX epoch
        val value3 = Calendar.getInstance()!!
        value3.set(1969, 7, 20, 20, 17, 3)
        value3.timeZone = TimeZone.getTimeZone("GMT-12")
        datetimeMetric.set(value3)
        // Check that data was properly recorded.
        assertTrue(datetimeMetric.testHasValue())
        assertEquals("1969-08-20T20:17-12:00", datetimeMetric.testGetValueAsString())

        // A date following 2038 (the extent of signed 32-bits after UNIX epoch)
        // This fails on some workers on Taskcluster.  32-bit platforms, perhaps?

        // val value4 = Calendar.getInstance()!!
        // value4.set(2039, 7, 20, 20, 17, 3)
        // datetimeMetric.set(value4)
        // // Check that data was properly recorded.
        // assertTrue(datetimeMetric.testHasValue())
        // assertEquals("2039-08-20T20:17:03-04:00", datetimeMetric.testGetValueAsString())
    }

    @Test
    fun `disabled datetimes must not record data`() {
        // Define a 'datetimeMetric' datetime metric, which will be stored in "store1". It's disabled
        // so it should not record anything.
        val datetimeMetric = DatetimeMetricType(
            disabled = true,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "datetimeMetric",
            sendInPings = listOf("store1")
        )

        // Attempt to store the datetime.
        datetimeMetric.set()
        assertFalse(datetimeMetric.testHasValue())
    }

    @Test
    fun `test that truncation works`() {
        val value = Calendar.getInstance()!!
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
        val expectedNanosecond = Calendar.getInstance()!!
        expectedNanosecond.timeZone = TimeZone.getTimeZone("GMT-2")
        expectedNanosecond.set(2004, 11, 9, 8, 3, 29)
        expectedNanosecond.set(Calendar.MILLISECOND, 320)
        assertEquals(expectedNanosecond.getTime(), datetimeNanosecondTruncation.testGetValue())
        assertEquals("2004-12-09T08:03:29.320-02:00", datetimeNanosecondTruncation.testGetValueAsString())

        val expectedMillisecond = Calendar.getInstance()!!
        expectedMillisecond.timeZone = TimeZone.getTimeZone("GMT-2")
        expectedMillisecond.set(2004, 11, 9, 8, 3, 29)
        expectedMillisecond.set(Calendar.MILLISECOND, 320)
        assertEquals(expectedMillisecond.getTime(), datetimeMillisecondTruncation.testGetValue())
        assertEquals("2004-12-09T08:03:29.320-02:00", datetimeMillisecondTruncation.testGetValueAsString())

        val expectedSecond = Calendar.getInstance()!!
        expectedSecond.timeZone = TimeZone.getTimeZone("GMT-2")
        expectedSecond.set(2004, 11, 9, 8, 3, 29)
        expectedSecond.set(Calendar.MILLISECOND, 0)
        assertEquals(expectedSecond.getTime(), datetimeSecondTruncation.testGetValue())
        assertEquals("2004-12-09T08:03:29-02:00", datetimeSecondTruncation.testGetValueAsString())

        val expectedMinute = Calendar.getInstance()!!
        expectedMinute.timeZone = TimeZone.getTimeZone("GMT-2")
        expectedMinute.set(2004, 11, 9, 8, 3, 0)
        expectedMinute.set(Calendar.MILLISECOND, 0)
        assertEquals(expectedMinute.getTime(), datetimeMinuteTruncation.testGetValue())
        assertEquals("2004-12-09T08:03-02:00", datetimeMinuteTruncation.testGetValueAsString())

        val expectedHour = Calendar.getInstance()!!
        expectedHour.timeZone = TimeZone.getTimeZone("GMT-2")
        expectedHour.set(2004, 11, 9, 8, 0, 0)
        expectedHour.set(Calendar.MILLISECOND, 0)
        assertEquals(expectedHour.getTime(), datetimeHourTruncation.testGetValue())
        assertEquals("2004-12-09T08-02:00", datetimeHourTruncation.testGetValueAsString())

        val expectedDay = Calendar.getInstance()!!
        expectedDay.timeZone = TimeZone.getTimeZone("GMT-2")
        expectedDay.set(2004, 11, 9, 0, 0, 0)
        expectedDay.set(Calendar.MILLISECOND, 0)
        assertEquals(expectedDay.getTime(), datetimeDayTruncation.testGetValue())
        assertEquals("2004-12-09-02:00", datetimeDayTruncation.testGetValueAsString())
    }
}
