/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean

import mozilla.components.service.glean.storages.StringsStorageEngine
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

import org.junit.Before
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class StringMetricTypeTest {

    @Before
    fun setUp() {
        StringsStorageEngine.clearAllStores()
    }

    @Test
    fun `The API saves to its storage engine`() {
        // Define a 'stringMetric' string metric, which will be stored in "store1"
        val stringMetric = StringMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "string_metric",
            sendInPings = listOf("store1")
        )

        // Record two strings of the same type, with a little delay.
        stringMetric.set("value")

        // Check that data was properly recorded.
        val snapshot = StringsStorageEngine.getSnapshot(storeName = "store1", clearStore = false)
        assertEquals(1, snapshot!!.size)
        assertEquals(true, snapshot.containsKey("telemetry.string_metric"))
        assertEquals("value", snapshot.get("telemetry.string_metric"))

        stringMetric.set("overriddenValue")
        // Check that data was properly recorded.
        val snapshot2 = StringsStorageEngine.getSnapshot(storeName = "store1", clearStore = false)
        assertEquals(1, snapshot2!!.size)
        assertEquals(true, snapshot2.containsKey("telemetry.string_metric"))
        assertEquals("overriddenValue", snapshot2.get("telemetry.string_metric"))
    }

    @Test
    fun `The API truncates long string values`() {
        // Define a 'stringMetric' string metric, which will be stored in "store1"
        val stringMetric = StringMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "string_metric",
            sendInPings = listOf("store1")
        )

        stringMetric.set("0123456789012345678901234567890123456789012345678901234567890123456789")
        // Check that data was truncated.
        val snapshot = StringsStorageEngine.getSnapshot(storeName = "store1", clearStore = false)
        assertEquals(1, snapshot!!.size)
        assertEquals(true, snapshot.containsKey("telemetry.string_metric"))
        assertEquals(
            "01234567890123456789012345678901234567890123456789",
            snapshot.get("telemetry.string_metric")
        )
    }

    @Test
    fun `strings with no lifetime must not record data`() {
        // Define a 'stringMetric' string metric, which will be stored in
        // "store1". It's disabled so it should not record anything.
        val stringMetric = StringMetricType(
            disabled = true,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "stringMetric",
            sendInPings = listOf("store1")
        )

        // Attempt to store the string.
        stringMetric.set("value")
        // Check that nothing was recorded.
        val snapshot = StringsStorageEngine.getSnapshot(storeName = "store1", clearStore = false)
        assertNull("Strings must not be recorded if they are disabled", snapshot)
    }

    @Test
    fun `disabled strings must not record data`() {
        // Define a 'stringMetric' string metric, which will be stored in "store1". It's disabled
        // so it should not record anything.
        val stringMetric = StringMetricType(
            disabled = true,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "stringMetric",
            sendInPings = listOf("store1")
        )

        // Attempt to store the string.
        stringMetric.set("value")
        // Check that nothing was recorded.
        val snapshot = StringsStorageEngine.getSnapshot(storeName = "store1", clearStore = false)
        assertNull("Strings must not be recorded if they are disabled", snapshot)
    }
}
