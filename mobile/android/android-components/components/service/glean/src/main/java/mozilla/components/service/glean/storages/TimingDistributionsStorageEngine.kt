/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.storages

import android.annotation.SuppressLint
import android.content.SharedPreferences
import android.support.annotation.VisibleForTesting
import mozilla.components.service.glean.CommonMetricData
import mozilla.components.service.glean.HistogramType
import mozilla.components.service.glean.TimeUnit
import mozilla.components.service.glean.error.ErrorRecording

import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.android.org.json.toList
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
) : GenericScalarStorageEngine<TimingDistributionData>() {

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
     * @param sample the value to accumulate
     */
    @Synchronized
    fun accumulate(
        metricData: CommonMetricData,
        sample: Long,
        timeUnit: TimeUnit
    ) {
        if (sample < 0) {
            ErrorRecording.recordError(
                metricData,
                ErrorRecording.ErrorType.InvalidValue,
                "Accumulate negative $sample",
                logger
            )
            return
        }

        // Since the custom combiner closure captures this value, we need to just create a dummy
        // value here that won't be used by the combine function, and create a fresh
        // TimingDistributionData for each value that doesn't have an existing current value.
        val dummy = TimingDistributionData(category = metricData.category, name = metricData.name,
            timeUnit = timeUnit)
        super.recordScalar(metricData, dummy, null) { currentValue, newValue ->
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
 * @param range an array always containing 2 elements: the minimum and maximum bucket values
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
    val range: List<Long> = listOf(DEFAULT_RANGE_MIN, DEFAULT_RANGE_MAX),
    val histogramType: HistogramType = HistogramType.Exponential,
    val values: MutableMap<String, Long> = mutableMapOf(),
    var sum: Long = 0,
    val timeUnit: TimeUnit = TimeUnit.Millisecond
) {
    companion object {
        // These represent the indexes of the min and max of the timing distribution range
        const val MIN = 0
        const val MAX = 1

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
        @Suppress("ReturnCount")
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
            val bucketCount = jsonObject.tryGetInt("bucketCount") ?: return null
            // If 'range' isn't present, JSONException is thrown
            val range = try {
                jsonObject.getJSONArray("range").toList<Long>()
            } catch (e: org.json.JSONException) {
                return null
            }
            val rawHistogramType = jsonObject.tryGetInt("histogramType") ?: return null
            val histogramType = HistogramType.values().getOrNull(rawHistogramType) ?: return null
            // Attempt to parse the values map, if it fails then something is wrong and we need to
            // return null.
            val values = try {
                val mapData = jsonObject.getJSONObject("values")
                val valueMap: MutableMap<String, Long> = mutableMapOf()
                mapData.keys().forEach { key ->
                    valueMap[key] = mapData.tryGetLong(key) ?: 0L
                }
                valueMap
            } catch (e: org.json.JSONException) {
                // This should only occur if there isn't a key/value pair stored for "values"
                return null
            }
            val sum = jsonObject.tryGetLong("sum") ?: return null
            val timeUnit = TimeUnit.values()[jsonObject.tryGetInt("timeUnit")
                ?: return null]

            return TimingDistributionData(
                category = category,
                name = name,
                bucketCount = bucketCount,
                range = range,
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
     * Accumulates a sample to the correct bucket, using a linear search to locate the index of the
     * bucket where the sample is less than or equal to the bucket value.  This works since the
     * buckets are sorted in ascending order.  If a mapped value doesn't exist for this bucket yet,
     * one is created.
     *
     * @param sample Long value representing the sample that is being accumulated
     */
    internal fun accumulate(sample: Long) {
        for (i in buckets.indices) {
            if (sample <= buckets[i] || i == bucketCount - 1) {
                values["$i"] = (values["$i"] ?: 0) + 1
                break
            }
        }

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
            "bucketCount" to bucketCount,
            "range" to JSONArray(range),
            "histogramType" to histogramType.ordinal,
            "values" to values,
            "sum" to sum,
            "timeUnit" to timeUnit.ordinal
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
        // `range[MAX]`
        val logMax = Math.log(range[MAX].toDouble())
        val result: MutableList<Long> = mutableListOf()
        var current = range[MIN]
        if (current == 0L) {
            current = 1L
        }
        result.add(current)
        for (i in 2..bucketCount) {
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
