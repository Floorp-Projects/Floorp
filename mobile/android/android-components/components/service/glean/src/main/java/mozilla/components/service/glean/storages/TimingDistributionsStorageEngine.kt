/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.storages

import android.annotation.SuppressLint
import android.content.SharedPreferences
import androidx.annotation.VisibleForTesting
import mozilla.components.service.glean.error.ErrorRecording
import mozilla.components.service.glean.private.CommonMetricData
import mozilla.components.service.glean.private.HistogramType
import mozilla.components.service.glean.private.TimeUnit
import mozilla.components.service.glean.utils.getAdjustedTime

import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.android.org.json.tryGetInt
import mozilla.components.support.ktx.android.org.json.tryGetLong
import mozilla.components.support.ktx.android.org.json.tryGetString
import org.json.JSONArray
import org.json.JSONObject

/**
 * This singleton handles the in-memory storage logic for timing distributions. It is meant to be
 * used by the Timing Distribution API and the ping assembling objects.
 *
 * This class contains a reference to the Android application Context. While the IDE warns
 * us that this could leak, the application context lives as long as the application and this
 * object. For this reason, we should be safe to suppress the IDE warning.
 */
@SuppressLint("StaticFieldLeak")
internal object TimingDistributionsStorageEngine : TimingDistributionsStorageEngineImplementation()

internal open class TimingDistributionsStorageEngineImplementation(
    override val logger: Logger = Logger("glean/TimingDistributionsStorageEngine")
) : GenericStorageEngine<TimingDistributionData>() {

    override fun deserializeSingleMetric(metricName: String, value: Any?): TimingDistributionData? {
        return try {
            (value as? String)?.let {
                TimingDistributionData.fromJsonString(it)
            }
        } catch (e: org.json.JSONException) {
            null
        }
    }

    override fun serializeSingleMetric(
        userPreferences: SharedPreferences.Editor?,
        storeName: String,
        value: TimingDistributionData,
        extraSerializationData: Any?
    ) {
        val json = value.toJsonObject()
        userPreferences?.putString(storeName, json.toString())
    }

    /**
     * Accumulate value for the provided metric.
     *
     * @param metricData the metric information for the timing distribution
     * @param sample the value to accumulate, in nanoseconds
     * @param timeUnit the [TimeUnit] the sample will be converted to
     */
    @Synchronized
    fun accumulate(
        metricData: CommonMetricData,
        sample: Long,
        timeUnit: TimeUnit
    ) {
        // We're checking for errors in `accumulateSamples` already, but
        // we need to check it here too anyway because `getAdjustedTime` would
        // throw otherwise.
        if (sample < 0) {
            ErrorRecording.recordError(
                metricData,
                ErrorRecording.ErrorType.InvalidValue,
                "Accumulate negative $sample",
                logger
            )
            return
        }

        val sampleInUnit = getAdjustedTime(timeUnit, sample)
        accumulateSamples(metricData, longArrayOf(sampleInUnit), timeUnit)
    }

    /**
     * Accumulate an array of samples for the provided metric.
     *
     * @param metricData the metric information for the timing distribution
     * @param samples the values to accumulate, provided in the metric's [TimeUnit] (they won't
     *        be truncated nor converted)
     * @param timeUnit the [TimeUnit] the samples are in
     */
    @Synchronized
    fun accumulateSamples(
        metricData: CommonMetricData,
        samples: LongArray,
        timeUnit: TimeUnit
    ) {
        val validSamples = samples.filter { sample -> sample >= 0 }
        val numNegativeSamples = samples.size - validSamples.size
        if (numNegativeSamples > 0) {
            ErrorRecording.recordError(
                metricData,
                ErrorRecording.ErrorType.InvalidValue,
                "Accumulate $numNegativeSamples negative samples",
                logger,
                numNegativeSamples
            )
            return
        }

        // Since the custom combiner closure captures this value, we need to just create a dummy
        // value here that won't be used by the combine function, and create a fresh
        // TimingDistributionData for each value that doesn't have an existing current value.
        val dummy = TimingDistributionData(category = metricData.category, name = metricData.name,
            timeUnit = timeUnit)
        validSamples.forEach { sample ->
            super.recordMetric(metricData, dummy, null) { currentValue, _ ->
                currentValue?.let {
                    it.accumulate(sample)
                    it
                } ?: let {
                    val newTD = TimingDistributionData(category = metricData.category, name = metricData.name,
                        timeUnit = timeUnit)
                    newTD.accumulate(sample)
                    return@let newTD
                }
            }
        }
    }

    /**
     * Get a snapshot of the stored data as a JSON object.
     *
     * @param storeName the name of the desired store
     * @param clearStore whether or not to clearStore the requested store
     *
     * @return the [JSONObject] containing the recorded data.
     */
    override fun getSnapshotAsJSON(storeName: String, clearStore: Boolean): Any? {
        return getSnapshot(storeName, clearStore)?.let { dataMap ->
            val jsonObj = JSONObject()
            dataMap.forEach {
                jsonObj.put(it.key, it.value.toJsonObject())
            }
            return jsonObj
        }
    }
}

/**
 * This class represents the structure of a timing distribution according to the pipeline schema. It
 * is meant to help serialize and deserialize data to the correct format for transport and storage,
 * as well as including a helper function to calculate the bucket sizes.
 *
 * @param category of the metric
 * @param name of the metric
 * @param bucketCount total number of buckets
 * @param rangeMin the minimum value that can be represented
 * @param rangeMax the maximum value that can be represented
 * @param histogramType the [HistogramType] representing the bucket layout
 * @param values a map containing the bucket index mapped to the accumulated count
 * @param sum the accumulated sum of all the samples in the timing distribution
 * @param timeUnit the base [TimeUnit] of the bucket values
 */
@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
data class TimingDistributionData(
    val category: String,
    val name: String,
    val bucketCount: Int = DEFAULT_BUCKET_COUNT,
    val rangeMin: Long = DEFAULT_RANGE_MIN,
    val rangeMax: Long = DEFAULT_RANGE_MAX,
    val histogramType: HistogramType = HistogramType.Exponential,
    // map from bucket limits to accumulated values
    val values: MutableMap<Long, Long> = mutableMapOf(),
    var sum: Long = 0,
    val timeUnit: TimeUnit = TimeUnit.Millisecond
) {
    companion object {
        // The following are defaults for a simple timing distribution for the default time unit
        // of millisecond.  The values arrived at were an approximated using existing "_MS"
        // telemetry probes as a guide.
        const val DEFAULT_BUCKET_COUNT = 100
        const val DEFAULT_RANGE_MIN = 0L
        const val DEFAULT_RANGE_MAX = 60000L

        /**
         * Factory function that takes stringified JSON and converts it back into a
         * [TimingDistributionData].  This tries to read all values and attempts to
         * use a default where no value exists.
         *
         * @param json Stringified JSON value representing a [TimingDistributionData] object
         * @return A [TimingDistributionData] or null if unable to rebuild from the string.
         */
        @Suppress("ReturnCount", "ComplexMethod")
        internal fun fromJsonString(json: String): TimingDistributionData? {
            val jsonObject: JSONObject
            try {
                jsonObject = JSONObject(json)
            } catch (e: org.json.JSONException) {
                return null
            }

            // Category can be empty so it may be possible to be a null value so try and allow this
            // by using `orEmpty()` to fill in the value.  Other values should be present or else
            // something is wrong and we should return null.
            val category = jsonObject.tryGetString("category").orEmpty()
            val name = jsonObject.tryGetString("name") ?: return null
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
            val rawTimeUnit = jsonObject.tryGetString("time_unit") ?: return null
            val timeUnit = try {
                TimeUnit.valueOf(rawTimeUnit.capitalize())
            } catch (e: IllegalArgumentException) {
                return null
            }

            return TimingDistributionData(
                category = category,
                name = name,
                bucketCount = bucketCount,
                rangeMin = range.getLong(0),
                rangeMax = range.getLong(1),
                histogramType = histogramType,
                values = values,
                sum = sum,
                timeUnit = timeUnit
            )
        }
    }

    // This is a calculated read-only property that returns the total count of accumulated values
    val count: Long
        get() = values.map { it.value }.sum()

    // This is a helper property to build the correct identifier for the metric and allow for
    // blank categories
    internal val identifier: String = if (category.isEmpty()) { name } else { "$category.$name" }

    // This is a list of limits for the buckets.  Instantiated lazily to ensure that the range and
    // bucket counts are set first.
    internal val buckets: List<Long> by lazy { getBuckets() }

    /**
     * Accumulates a sample to the correct bucket, using a binary search to locate the index of the
     * bucket where the sample is bigger than or equal to the bucket limit.
     * If a value doesn't exist for this bucket yet, one is created.
     *
     * @param sample Long value representing the sample that is being accumulated
     */
    internal fun accumulate(sample: Long) {
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

        val limit = buckets[mid]
        values[limit] = (values[limit] ?: 0) + 1
        sum += sample
    }

    /**
     * Helper function to build the [TimingDistributionData] into a JSONObject for serialization
     * purposes.
     */
    internal fun toJsonObject(): JSONObject {
        return JSONObject(mapOf(
            "category" to category,
            "name" to name,
            "bucket_count" to bucketCount,
            "range" to JSONArray(arrayOf(rangeMin, rangeMax)),
            "histogram_type" to histogramType.toString().toLowerCase(),
            "values" to values.mapKeys { "${it.key}" },
            "sum" to sum,
            "time_unit" to timeUnit.toString().toLowerCase()
        ))
    }

    /**
     * Helper function to generate the list of bucket max values used when accumulating to the
     * correct buckets.
     *
     * @return List containing the bucket limits
     */
    private fun getBuckets(): List<Long> {
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
}
