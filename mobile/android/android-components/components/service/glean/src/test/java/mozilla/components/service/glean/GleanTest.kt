/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import mozilla.components.service.glean.private.BooleanMetricType
import mozilla.components.service.glean.private.CommonMetricData
import mozilla.components.service.glean.private.Lifetime
import mozilla.components.service.glean.testing.GleanTestRule
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class GleanTest {
    private val context: Context
        get() = ApplicationProvider.getApplicationContext()

    @get:Rule
    val gleanRule = GleanTestRule(context)

    @Test
    fun `Glean correctly initializes and records a metric`() {
        // Define a 'booleanMetric' boolean metric, which will be stored in "store1"
        val booleanMetric = BooleanMetricType(
            CommonMetricData(
                disabled = false,
                category = "telemetry",
                lifetime = Lifetime.APPLICATION,
                name = "boolean_metric",
                sendInPings = listOf("store1"),
            ),
        )

        booleanMetric.set(true)

        assertTrue(booleanMetric.testGetValue()!!)
    }
}
