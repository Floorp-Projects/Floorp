/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean

import kotlinx.coroutines.ExperimentalCoroutinesApi
import kotlinx.coroutines.ObsoleteCoroutinesApi
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Assert.assertFalse
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import java.lang.NullPointerException

@ObsoleteCoroutinesApi
@ExperimentalCoroutinesApi
@RunWith(RobolectricTestRunner::class)
class BooleanMetricTypeTest {

    @Before
    fun setUp() {
        resetGlean()
    }

    @Test
    fun `The API must define the expected "default" storage`() {
        // Define a 'booleanMetric' boolean metric, which will be stored in "store1"
        val booleanMetric = BooleanMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "boolean_metric",
            sendInPings = listOf("store1")
        )
        assertEquals(listOf("metrics"), booleanMetric.defaultStorageDestinations)
    }

    @Test
    fun `The API saves to its storage engine`() {
        // Define a 'booleanMetric' boolean metric, which will be stored in "store1"
        val booleanMetric = BooleanMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "boolean_metric",
            sendInPings = listOf("store1")
        )

        // Record two booleans of the same type, with a little delay.
        booleanMetric.set(true)
        // Check that data was properly recorded.
        assertTrue(booleanMetric.testHasValue())
        assertTrue(booleanMetric.testGetValue())

        booleanMetric.set(false)
        // Check that data was properly recorded.
        assertTrue(booleanMetric.testHasValue())
        assertFalse(booleanMetric.testGetValue())
    }

    @Test
    fun `disabled booleans must not record data`() {
        // Define a 'booleanMetric' boolean metric, which will be stored in "store1". It's disabled
        // so it should not record anything.
        val booleanMetric = BooleanMetricType(
            disabled = true,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "booleanMetric",
            sendInPings = listOf("store1")
        )

        // Attempt to store the boolean.
        booleanMetric.set(true)
        // Check that nothing was recorded.
        assertFalse(booleanMetric.testHasValue())
    }

    @Test(expected = NullPointerException::class)
    fun `testGetValue() throws NullPointerException if nothing is stored`() {
        // Define a 'booleanMetric' boolean metric to have an instance to call
        // testGetValue() on
        val booleanMetric = BooleanMetricType(
            disabled = true,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "booleanMetric",
            sendInPings = listOf("store1")
        )
        booleanMetric.testGetValue()
    }

    @Test
    fun `The API saves to secondary pings`() {
        // Define a 'booleanMetric' boolean metric, which will be stored in "store1" and "store2"
        val booleanMetric = BooleanMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "boolean_metric",
            sendInPings = listOf("store1", "store2")
        )

        // Record two booleans of the same type, with a little delay.
        booleanMetric.set(true)
        // Check that data was properly recorded in the second ping
        assertTrue(booleanMetric.testHasValue("store2"))
        assertTrue(booleanMetric.testGetValue("store2"))

        booleanMetric.set(false)
        // Check that data was properly recorded in the second ping.
        assertTrue(booleanMetric.testHasValue("store2"))
        assertFalse(booleanMetric.testGetValue("store2"))
    }
}
