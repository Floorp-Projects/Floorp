/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.histogram

import androidx.test.core.app.ApplicationProvider
import mozilla.components.service.glean.testing.GleanTestRule
import org.junit.Assert.assertEquals
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner
import java.lang.Math.pow

@RunWith(RobolectricTestRunner::class)
class FunctionalHistogramTest {

    @get:Rule
    val gleanRule = GleanTestRule(ApplicationProvider.getApplicationContext())

    @Test
    fun `sampleToBucketMinimum correctly rounds down`() {
        val hist = FunctionalHistogram(2.0, 8.0)

        // Check each of the first 100 integers, where numerical accuracy of the round-tripping
        // is most potentially problematic
        for (i in (0..100)) {
            val value = i.toLong()
            val bucketMinimum = hist.sampleToBucketMinimum(value)
            assert(bucketMinimum <= value)

            assertEquals(bucketMinimum, hist.sampleToBucketMinimum(bucketMinimum))
        }

        // Do an exponential sampling of higher numbers
        for (i in (11..500)) {
            val value = pow(1.5, i.toDouble()).toLong()
            val bucketMinimum = hist.sampleToBucketMinimum(value)
            assert(bucketMinimum <= value)

            assertEquals(bucketMinimum, hist.sampleToBucketMinimum(bucketMinimum))
        }
    }

    @Test
    fun `toJsonObject correctly converts a FunctionalHistogram object`() {
        // Define a FunctionalHistogram object
        val tdd = FunctionalHistogram(2.0, 8.0)

        // Accumulate some samples to populate sum and values properties
        tdd.accumulate(1L)
        tdd.accumulate(2L)
        tdd.accumulate(3L)

        // Convert to JSON object using toJsonObject()
        val jsonTdd = tdd.toJsonPayloadObject()

        // Verify properties
        val jsonValue = jsonTdd.getJSONObject("values")
        assertEquals("JSON values must match Timing Distribution values",
        tdd.values[1], jsonValue.getLong("1"))
        assertEquals("JSON values must match Timing Distribution values",
        tdd.values[3], jsonValue.getLong("3"))
        assertEquals("JSON values must match Timing Distribution values",
        0, jsonValue.getLong("4"))
        assertEquals("JSON sum must match Timing Distribution sum",
        tdd.sum, jsonTdd.getLong("sum"))
    }
}
