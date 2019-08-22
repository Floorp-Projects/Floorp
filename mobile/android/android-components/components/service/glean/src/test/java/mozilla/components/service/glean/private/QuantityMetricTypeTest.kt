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
import java.lang.NullPointerException

@ObsoleteCoroutinesApi
@ExperimentalCoroutinesApi
@RunWith(RobolectricTestRunner::class)
class QuantityMetricTypeTest {

    @get:Rule
    val gleanRule = GleanTestRule(ApplicationProvider.getApplicationContext())

    @Test
    fun `The API saves to its storage engine`() {
        // Define a 'quantityMetric' quantity metric, which will be stored in "store1"
        val quantityMetric = QuantityMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "quantity_metric",
            sendInPings = listOf("store1")
        )

        // Set the quantity a couple of times.
        quantityMetric.set(1L)

        assertTrue(quantityMetric.testHasValue())
        assertEquals(1, quantityMetric.testGetValue())

        quantityMetric.set(10L)
        assertTrue(quantityMetric.testHasValue())
        assertEquals(10, quantityMetric.testGetValue())
    }

    @Test
    fun `quantitys with no lifetime must not record data`() {
        // Define a 'quantityMetric' quantity metric, which will be stored in "store1".
        // It's disabled so it should not record anything.
        val quantityMetric = QuantityMetricType(
            disabled = true,
            category = "telemetry",
            lifetime = Lifetime.Ping,
            name = "quantity_metric",
            sendInPings = listOf("store1")
        )

        quantityMetric.set(1L)
        // Check that nothing was recorded.
        assertFalse("Quantitys must not be recorded if they are disabled",
            quantityMetric.testHasValue())
    }

    @Test
    fun `disabled quantitys must not record data`() {
        // Define a 'quantityMetric' quantity metric, which will be stored in "store1".  It's disabled
        // so it should not record anything.
        val quantityMetric = QuantityMetricType(
            disabled = true,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "quantity_metric",
            sendInPings = listOf("store1")
        )

        // Attempt to store the quantity.
        quantityMetric.set(1L)
        // Check that nothing was recorded.
        assertFalse("Quantitys must not be recorded if they are disabled",
            quantityMetric.testHasValue())
    }

    @Test(expected = NullPointerException::class)
    fun `testGetValue() throws NullPointerException if nothing is stored`() {
        val quantityMetric = QuantityMetricType(
            disabled = true,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "quantity_metric",
            sendInPings = listOf("store1")
        )
        quantityMetric.testGetValue()
    }

    @Test
    fun `The API saves to secondary pings`() {
        // Define a 'quantityMetric' quantity metric, which will be stored in "store1" and "store2"
        val quantityMetric = QuantityMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "quantity_metric",
            sendInPings = listOf("store1", "store2")
        )

        // Add to the quantity a couple of times.
        quantityMetric.set(1L)

        assertTrue(quantityMetric.testHasValue("store2"))
        assertEquals(1L, quantityMetric.testGetValue("store2"))

        quantityMetric.set(10L)
        assertTrue(quantityMetric.testHasValue("store2"))
        assertEquals(10L, quantityMetric.testGetValue("store2"))
    }
}
