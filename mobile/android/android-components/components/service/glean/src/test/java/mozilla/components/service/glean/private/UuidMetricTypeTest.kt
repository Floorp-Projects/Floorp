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
import java.util.UUID

@ObsoleteCoroutinesApi
@ExperimentalCoroutinesApi
@RunWith(RobolectricTestRunner::class)
class UuidMetricTypeTest {

    @get:Rule
    val gleanRule = GleanTestRule(ApplicationProvider.getApplicationContext())

    @Test
    fun `The API saves to its storage engine`() {
        // Define a 'uuidMetric' uuid metric, which will be stored in "store1"
        val uuidMetric = UuidMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "uuid_metric",
            sendInPings = listOf("store1")
        )

        // Record two uuids of the same type, with a little delay.
        val uuid = uuidMetric.generateAndSet()

        // Check that data was properly recorded.
        assertTrue(uuidMetric.testHasValue())
        assertEquals(uuid, uuidMetric.testGetValue())

        val uuid2 = UUID.fromString("ce2adeb8-843a-4232-87a5-a099ed1e7bb3")
        uuidMetric.set(uuid2)

        // Check that data was properly recorded.
        assertTrue(uuidMetric.testHasValue())
        assertEquals(uuid2, uuidMetric.testGetValue())
    }

    @Test
    fun `uuids with no lifetime must not record data`() {
        // Define a 'uuidMetric' uuid metric, which will be stored in
        // "store1". It's disabled so it should not record anything.
        val uuidMetric = UuidMetricType(
            disabled = true,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "uuidMetric",
            sendInPings = listOf("store1")
        )

        // Attempt to store the uuid.
        uuidMetric.generateAndSet()
        // Check that nothing was recorded.
        assertFalse("Uuids must not be recorded if they have no lifetime",
            uuidMetric.testHasValue())
    }

    @Test
    fun `disabled uuids must not record data`() {
        // Define a 'uuidMetric' uuid metric, which will be stored in "store1". It's disabled
        // so it should not record anything.
        val uuidMetric = UuidMetricType(
            disabled = true,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "uuidMetric",
            sendInPings = listOf("store1")
        )

        // Attempt to store the uuid.
        uuidMetric.generateAndSet()
        // Check that nothing was recorded.
        assertFalse("Uuids must not be recorded if they are disabled",
            uuidMetric.testHasValue())
    }

    @Test(expected = NullPointerException::class)
    fun `testGetValue() throws NullPointerException if nothing is stored`() {
        val uuidMetric = UuidMetricType(
            disabled = true,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "uuidMetric",
            sendInPings = listOf("store1")
        )
        uuidMetric.testGetValue()
    }

    @Test
    fun `The API saves to secondary pings`() {
        // Define a 'uuidMetric' uuid metric, which will be stored in "store1" and "store2"
        val uuidMetric = UuidMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "uuid_metric",
            sendInPings = listOf("store1", "store2")
        )

        // Record two uuids of the same type, with a little delay.
        val uuid = uuidMetric.generateAndSet()

        // Check that data was properly recorded.
        assertTrue(uuidMetric.testHasValue("store2"))
        assertEquals(uuid, uuidMetric.testGetValue("store2"))

        val uuid2 = UUID.fromString("ce2adeb8-843a-4232-87a5-a099ed1e7bb3")
        uuidMetric.set(uuid2)

        // Check that data was properly recorded.
        assertTrue(uuidMetric.testHasValue("store2"))
        assertEquals(uuid2, uuidMetric.testGetValue("store2"))
    }
}
