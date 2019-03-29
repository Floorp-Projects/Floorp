/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.metrics

import org.junit.Assert.assertEquals
import org.junit.Test

import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

/**
 * The following is a test-only specific metric type used
 * to test the "default" data storage translation.
 */
data class TestOnlyMetricType(
    override val disabled: Boolean,
    override val category: String,
    override val lifetime: Lifetime,
    override val name: String,
    override val sendInPings: List<String>
) : CommonMetricData {

    override val defaultStorageDestinations: List<String> = listOf("translated_default")
}

@RunWith(RobolectricTestRunner::class)
class CommonMetricDataTest {
    @Test
    fun `getStorageNames() should correctly translate "default"`() {
        val testData = TestOnlyMetricType(
            disabled = false,
            category = "glean_test",
            lifetime = Lifetime.Ping,
            name = "test",
            sendInPings = listOf(CommonMetricData.DEFAULT_STORAGE_NAME, "other_test_ping"))

        assertEquals(testData.getStorageNames(), listOf("other_test_ping", "translated_default"))
    }

    @Test
    fun `getStorageNames() should pass-through if no "default" is present`() {
        val testData = TestOnlyMetricType(
            disabled = false,
            category = "glean_test",
            lifetime = Lifetime.Ping,
            name = "test",
            sendInPings = listOf("other_test_ping", "additional_storage"))

        assertEquals(testData.getStorageNames(), listOf("other_test_ping", "additional_storage"))
    }
}
