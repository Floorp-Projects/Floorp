/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.histogram

import androidx.test.core.app.ApplicationProvider
import mozilla.components.service.glean.private.HistogramType
import mozilla.components.service.glean.testing.GleanTestRule
import org.junit.Assert.assertEquals
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class PrecomputedHistogramTest {
    @get:Rule
    val gleanRule = GleanTestRule(ApplicationProvider.getApplicationContext())

    @Test
    fun `samples go in the correct bucket for exponential bucketing`() {
        val td = PrecomputedHistogram(
            rangeMin = 0L,
            rangeMax = 60000L,
            bucketCount = 100,
            histogramType = HistogramType.Exponential
        )

        for (i in (0..60)) {
            val x = i * 1000L
            val bucket = td.findBucket(x)
            assert(bucket <= x)
        }

        val td2 = PrecomputedHistogram(
            rangeMin = 10000L,
            rangeMax = 50000L,
            bucketCount = 50,
            histogramType = HistogramType.Exponential
        )

        for (i in (0..60)) {
            val x = i * 1000L
            val bucket = td2.findBucket(x)
            assert(bucket <= x)
        }
    }

    @Test
    fun `validate the generated linear buckets`() {
        val td = PrecomputedHistogram(
            rangeMin = 0L,
            rangeMax = 99L,
            bucketCount = 100,
            histogramType = HistogramType.Linear
        )
        assertEquals(100, td.buckets.size)

        for (i in (0L until 100L)) {
            val bucket = td.findBucket(i)
            assert(bucket <= i)
        }

        val td2 = PrecomputedHistogram(
            rangeMin = 50L,
            rangeMax = 50000L,
            bucketCount = 50,
            histogramType = HistogramType.Linear
        )
        assertEquals(50, td2.buckets.size)

        for (i in (0..60)) {
            val x: Long = i * 1000L
            val bucket = td2.findBucket(x)
            assert(bucket <= x)
        }
    }

    @Test
    fun `toJsonObject and toJsonPayloadObject correctly converts a PrecomputedHistogram object`() {
        // Define a PrecomputedHistogram object
        val tdd = PrecomputedHistogram(
            rangeMin = 0L,
            rangeMax = 60000L,
            bucketCount = 100,
            histogramType = HistogramType.Exponential
        )

        // Accumulate some samples to populate sum and values properties
        tdd.accumulate(1L)
        tdd.accumulate(2L)
        tdd.accumulate(3L)

        // Convert to JSON object using toJsonObject()
        val jsonTdd = tdd.toJsonObject()

        // Verify properties
        assertEquals("JSON bucket count must match Precomputed Histogram bucket count",
            tdd.bucketCount, jsonTdd.getInt("bucket_count"))
        val jsonRange = jsonTdd.getJSONArray("range")
        assertEquals("JSON range minimum must match Precomputed Histogram range minimum",
            tdd.rangeMin, jsonRange.getLong(0))
        assertEquals("JSON range maximum must match Precomputed Histogram range maximum",
            tdd.rangeMax, jsonRange.getLong(1))
        assertEquals("JSON histogram type must match Precomputed Histogram histogram type",
            tdd.histogramType.toString().toLowerCase(), jsonTdd.getString("histogram_type"))
        val jsonValue = jsonTdd.getJSONObject("values")
        assertEquals("JSON values must match Precomputed Histogram values",
            tdd.values[1], jsonValue.getLong("1"))
        assertEquals("JSON values must match Precomputed Histogram values",
            tdd.values[2], jsonValue.getLong("2"))
        assertEquals("JSON values must match Precomputed Histogram values",
            tdd.values[3], jsonValue.getLong("3"))
        assertEquals("JSON sum must match Precomputed Histogram sum",
            tdd.sum, jsonTdd.getLong("sum"))

        // Convert to JSON object using toJsonObject()
        val jsonPayload = tdd.toJsonPayloadObject()

        // Verify properties
        assertEquals(2, jsonPayload.length())
        val jsonPayloadValue = jsonPayload.getJSONObject("values")
        assertEquals("JSON values must match Precomputed Histogram values",
            0, jsonPayloadValue.getLong("0"))
        assertEquals("JSON values must match Precomputed Histogram values",
            tdd.values[1], jsonPayloadValue.getLong("1"))
        assertEquals("JSON values must match Precomputed Histogram values",
            tdd.values[2], jsonPayloadValue.getLong("2"))
        assertEquals("JSON values must match Precomputed Histogram values",
            tdd.values[3], jsonPayloadValue.getLong("3"))
        assertEquals("JSON values must match Precomputed Histogram values",
            0, jsonPayloadValue.getLong("4"))
        assertEquals("JSON sum must match Precomputed Histogram sum",
            tdd.sum, jsonPayload.getLong("sum"))
    }
}
