/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.histogram

import androidx.annotation.VisibleForTesting
import mozilla.components.service.glean.private.HistogramType
import mozilla.components.support.ktx.android.org.json.tryGetInt
import mozilla.components.support.ktx.android.org.json.tryGetLong
import mozilla.components.support.ktx.android.org.json.tryGetString
import org.json.JSONArray
import org.json.JSONObject

/**
 * This class represents the structure of a custom distribution. It is meant
 * to help serialize and deserialize data to the correct format for transport and
 * storage, as well as including helper functions to calculate the bucket sizes.
 *
 * @param bucketCount total number of buckets
 * @param rangeMin the minimum value that can be represented
 * @param rangeMax the maximum value that can be represented
 * @param histogramType the [HistogramType] representing the bucket layout
 * @param values a map containing the bucket index mapped to the accumulated count
 * @param sum the accumulated sum of all the samples in the custom distribution
 */
@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
data class PrecomputedHistogram(
    val rangeMin: Long,
    val rangeMax: Long,
    val bucketCount: Int,
    val histogramType: HistogramType,
    // map from bucket limits to accumulated values
    val values: MutableMap<Long, Long> = mutableMapOf(),
    var sum: Long = 0
) {
    companion object {
        /**
         * Factory function that takes stringified JSON and converts it back into a
         * [PrecomputedHistogram].
         *
         * @param json Stringified JSON value representing a [PrecomputedHistogram] object
         * @return A [PrecomputedHistogram] or null if unable to rebuild from the string.
         */
        @Suppress("ReturnCount", "ComplexMethod")
        internal fun fromJsonString(json: String): PrecomputedHistogram? {
            val jsonObject: JSONObject
            try {
                jsonObject = JSONObject(json)
            } catch (e: org.json.JSONException) {
                return null
            }

            val bucketCount = jsonObject.tryGetInt("bucket_count") ?: return null
            // If 'range' isn't present, JSONException is thrown
            val range = try {
                val array = jsonObject.getJSONArray("range")
                // Range must have exactly 2 values
                if (array.length() == 2) {
                    // The getLong() function throws JSONException if we can't convert to a Long, so
                    // the catch should return null if either value isn't a valid Long
                    array.getLong(0)
                    array.getLong(1)
                    // This returns the JSONArray to the assignment if everything checks out
                    array
                } else {
                    return null
                }
            } catch (e: org.json.JSONException) {
                return null
            }
            val rawHistogramType = jsonObject.tryGetString("histogram_type") ?: return null
            val histogramType = try {
                HistogramType.valueOf(rawHistogramType.capitalize())
            } catch (e: IllegalArgumentException) {
                return null
            }
            // Attempt to parse the values map, if it fails then something is wrong and we need to
            // return null.
            val values = try {
                val mapData = jsonObject.getJSONObject("values")
                val valueMap: MutableMap<Long, Long> = mutableMapOf()
                mapData.keys().forEach { key ->
                    valueMap[key.toLong()] = mapData.tryGetLong(key) ?: 0L
                }
                valueMap
            } catch (e: org.json.JSONException) {
                // This should only occur if there isn't a key/value pair stored for "values"
                return null
            }
            val sum = jsonObject.tryGetLong("sum") ?: return null

            return PrecomputedHistogram(
                bucketCount = bucketCount,
                rangeMin = range.getLong(0),
                rangeMax = range.getLong(1),
                histogramType = histogramType,
                values = values,
                sum = sum
            )
        }
    }

    // This is a calculated read-only property that returns the total count of accumulated values
    val count: Long
        get() = values.map { it.value }.sum()

    // This is a list of limits for the buckets.  Instantiated lazily to ensure that the range and
    // bucket counts are set first.
    internal val buckets: List<Long> by lazy { getBuckets() }

    /**
     * Finds the correct bucket, using a binary search to locate the index of the
     * bucket where the sample is bigger than or equal to the bucket limit.
     *
     * @param sample Long value representing the sample that is being accumulated
     */
    internal fun findBucket(sample: Long): Long {
        var under = 0
        var over = bucketCount
        var mid: Int

        do {
            mid = under + (over - under) / 2
            if (mid == under) {
                break
            }

            if (buckets[mid] <= sample) {
                under = mid
            } else {
                over = mid
            }
        } while (true)

        return buckets[mid]
    }

    /**
     * Accumulates a sample to the correct bucket.
     * If a value doesn't exist for this bucket yet, one is created.
     *
     * @param sample Long value representing the sample that is being accumulated
     */
    internal fun accumulate(sample: Long) {
        val limit = findBucket(sample)
        values[limit] = (values[limit] ?: 0) + 1
        sum += sample
    }

    /**
     * Helper function to build the [PrecomputedHistogram] into a JSONObject for serialization
     * purposes.
     */
    internal fun toJsonObject(): JSONObject {
        return JSONObject(mapOf(
            "bucket_count" to bucketCount,
            "range" to JSONArray(arrayOf(rangeMin, rangeMax)),
            "histogram_type" to histogramType.toString().toLowerCase(),
            "values" to values.mapKeys { "${it.key}" },
            "sum" to sum
        ))
    }

    /**
     * Helper function to build the [PrecomputedHistogram] into a JSONObject for sending in the
     * ping payload. Compared to [toJsonObject] which is designed for lossless roundtripping:
     *
     *   - this does not include the bucketing parameters
     *   - all buckets [0, max) are inserted into values
     */
    internal fun toJsonPayloadObject(): JSONObject {
        // Include all buckets [0, max), where max is the maximum bucket with
        // any value recorded.
        val contiguousValues = if (!values.isEmpty()) {
            val bucketMax = values.keys.max()!!
            val contiguousValues = mutableMapOf<String, Long>()
            for (bucketMin in buckets) {
                contiguousValues["$bucketMin"] = values.getOrElse(bucketMin) { 0L }
                if (bucketMin > bucketMax) {
                    break
                }
            }
            contiguousValues
        } else {
            values
        }

        return JSONObject(mapOf(
            "values" to contiguousValues,
            "sum" to sum
        ))
    }

    /**
     * Helper function to generate the list of linear bucket min values used when accumulating
     * to the correct buckets.
     *
     * @return List containing the bucket limits
     */
    @Suppress("MagicNumber")
    private fun getBucketsLinear(): List<Long> {
        // Written to match the bucket generation on legacy desktop telemetry:
        //   https://searchfox.org/mozilla-central/rev/e0b0c38ee83f99d3cf868bad525ace4a395039f1/toolkit/components/telemetry/build_scripts/mozparsers/parse_histograms.py#65

        val result: MutableList<Long> = mutableListOf(0L)

        val dmin = rangeMin.toDouble()
        val dmax = rangeMax.toDouble()

        for (i in (1 until bucketCount)) {
            val linearRange = (dmin * (bucketCount - 1 - i) + dmax * (i - 1)) / (bucketCount - 2)
            result.add((linearRange + 0.5).toLong())
        }

        return result
    }

    /**
     * Helper function to generate the list of exponential bucket min values used when accumulating
     * to the correct buckets.
     *
     * @return List containing the bucket limits
     */
    private fun getBucketsExponential(): List<Long> {
        // Written to match the bucket generation on legacy desktop telemetry:
        //   https://searchfox.org/mozilla-central/rev/e0b0c38ee83f99d3cf868bad525ace4a395039f1/toolkit/components/telemetry/build_scripts/mozparsers/parse_histograms.py#75

        // This algorithm calculates the bucket sizes using a natural log approach to get
        // `bucketCount` number of buckets, exponentially spaced between `range[MIN]` and
        // `range[MAX]`.
        //
        // Bucket limits are the minimal bucket value.
        // That means values in a bucket `i` are `range[i] <= value < range[i+1]`.
        // It will always contain an underflow bucket (`< 1`).
        val logMax = Math.log(rangeMax.toDouble())
        val result: MutableList<Long> = mutableListOf()
        var current = rangeMin
        if (current == 0L) {
            current = 1L
        }

        // underflow bucket
        result.add(0)
        result.add(current)

        for (i in 2 until bucketCount) {
            val logCurrent = Math.log(current.toDouble())
            val logRatio = (logMax - logCurrent) / (bucketCount - i)
            val logNext = logCurrent + logRatio
            val nextValue = Math.round(Math.exp(logNext))
            if (nextValue > current) {
                current = nextValue
            } else {
                ++current
            }
            result.add(current)
        }
        return result.sorted()
    }

    /**
     * Helper function to generate the list of bucket min values used when accumulating
     * to the correct buckets.
     *
     * @return List containing the bucket limits
     */
    private fun getBuckets(): List<Long> {
        return when (histogramType) {
            HistogramType.Linear -> getBucketsLinear()
            HistogramType.Exponential -> getBucketsExponential()
        }
    }
}
