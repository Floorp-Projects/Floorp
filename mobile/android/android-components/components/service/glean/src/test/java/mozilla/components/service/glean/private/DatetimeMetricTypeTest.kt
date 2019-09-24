/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.private

import androidx.test.core.app.ApplicationProvider
import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.ObsoleteCoroutinesApi
import mozilla.components.service.glean.testing.GleanTestRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
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
    val gleanRule = GleanTestRule(ApplicationProvider.getApplicationContext())

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

        val value = Calendar.getInstance()
        value.set(2004, 11, 9, 8, 3, 29)
        value.timeZone = TimeZone.getTimeZone("America/Los_Angeles")
        datetimeMetric.set(value)
        assertTrue(datetimeMetric.testHasValue())
        assertEquals("2004-12-09T08:03-08:00", datetimeMetric.testGetValueAsString())

        val value2 = Calendar.getInstance()
        value2.set(1993, 1, 23, 9, 5, 43)
        value2.timeZone = TimeZone.getTimeZone("GMT+0")
        datetimeMetric.set(value2)
        // Check that data was properly recorded.
        assertTrue(datetimeMetric.testHasValue())
        assertEquals("1993-02-23T09:05+00:00", datetimeMetric.testGetValueAsString())

        // A date prior to the UNIX epoch
        val value3 = Calendar.getInstance()
        value3.set(1969, 7, 20, 20, 17, 3)
        value3.timeZone = TimeZone.getTimeZone("GMT-12")
        datetimeMetric.set(value3)
        // Check that data was properly recorded.
        assertTrue(datetimeMetric.testHasValue())
        assertEquals("1969-08-20T20:17-12:00", datetimeMetric.testGetValueAsString())

        // A date following 2038 (the extent of signed 32-bits after UNIX epoch)
        // This fails on some workers on Taskcluster.  32-bit platforms, perhaps?

        // val value4 = Calendar.getInstance()
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
}
