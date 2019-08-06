/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.storages

import android.annotation.SuppressLint
import android.content.SharedPreferences
import androidx.annotation.VisibleForTesting
import mozilla.components.service.glean.error.ErrorRecording
import mozilla.components.service.glean.private.CommonMetricData
import mozilla.components.service.glean.private.TimeUnit
import mozilla.components.service.glean.utils.timeToNanos

import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.android.org.json.tryGetLong
import mozilla.components.support.ktx.android.org.json.tryGetString
import org.json.JSONObject
import java.lang.Math.log
import java.lang.Math.pow
import kotlin.math.log

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

    companion object {
        // Maximum time of 10 minutes in nanoseconds
        internal const val MAX_SAMPLE_TIME: Long = 1000L * 1000L * 1000L * 60L * 10L
    }

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
     */
    @Synchronized
    fun accumulate(
        metricData: CommonMetricData,
        sample: Long
    ) {
        accumulateSamples(metricData, longArrayOf(sample))
    }

    /**
     * Accumulate an array of samples for the provided metric.
     *
     * @param metricData the metric information for the timing distribution
     * @param samples the values to accumulate, in the given `timeUnit`
     * @param timeUnit the unit that the given samples are in, defaults to nanoseconds
     */
    @Suppress("ComplexMethod")
    @Synchronized
    fun accumulateSamples(
        metricData: CommonMetricData,
        samples: LongArray,
        timeUnit: TimeUnit = TimeUnit.Nanosecond
    ) {
        // Remove invalid samples, and convert to nanos
        var numTooLongSamples = 0
        var numNegativeSamples = 0
        var factor = timeToNanos(timeUnit, 1)
        val validSamples = samples.map { sample ->
            if (sample < 0) {
                numNegativeSamples += 1
                0
            } else {
                val sampleInNanos = sample * factor
                if (sampleInNanos > MAX_SAMPLE_TIME) {
                    numTooLongSamples += 1
                    MAX_SAMPLE_TIME
                } else {
                    sampleInNanos
                }
            }
        }

        if (numNegativeSamples > 0) {
            ErrorRecording.recordError(
                metricData,
                ErrorRecording.ErrorType.InvalidValue,
                "Accumulate $numNegativeSamples negative samples",
                logger,
                numNegativeSamples
            )
            // Negative samples indicate a serious and unexpected error, so don't record anything
            return
        }

        if (numTooLongSamples > 0) {
            ErrorRecording.recordError(
                metricData,
                ErrorRecording.ErrorType.InvalidValue,
                "Accumulate $numTooLongSamples samples longer than 10 minutes",
                logger,
                numTooLongSamples
            )
            // Too long samples should just be truncated, but otherwise we deal record and handle them
        }

        // Since the custom combiner closure captures this value, we need to just create a dummy
        // value here that won't be used by the combine function, and create a fresh
        // TimingDistributionData for each value that doesn't have an existing current value.
        val dummy = TimingDistributionData(category = metricData.category, name = metricData.name)
        validSamples.forEach { sample ->
            super.recordMetric(metricData, dummy, null) { currentValue, _ ->
                currentValue?.let {
                    it.accumulate(sample)
                    it
                } ?: let {
                    val newTD = TimingDistributionData(category = metricData.category, name = metricData.name)
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
 * as well as performing the calculations to determine the correct bucket for each sample.
 *
 * The bucket index of a given sample is determined with the following function:
 *
 *     i = ⌊n log₂(𝑥)⌋
 *
 * In other words, there are n buckets for each power of 2 magnitude.
 *
 * The value of 8 for n was determined experimentally based on existing data to have sufficient
 * resolution.
 *
 * Samples greater than 10 minutes in length are truncated to 10 minutes.
 *
 * @param category of the metric
 * @param name of the metric
 * @param values a map containing the minimum bucket value mapped to the accumulated count
 * @param sum the accumulated sum of all the samples in the timing distribution
 */
@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
data class TimingDistributionData(
    val category: String,
    val name: String,
    // map from bucket limits to accumulated values
    val values: MutableMap<Long, Long> = mutableMapOf(),
    var sum: Long = 0
) {
    companion object {
        // The base of the logarithm used to determine bucketing
        internal const val LOG_BASE = 2.0

        // The buckets per each order of magnitude of the logarithm.
        internal const val BUCKETS_PER_MAGNITUDE = 8.0

        // The combined log base and buckets per magnitude.
        internal val EXPONENT = pow(LOG_BASE, 1.0 / BUCKETS_PER_MAGNITUDE)

        /**
         * Maps a sample to a "bucket index" that it belongs in.
         * A "bucket index" is the consecutive integer index of each bucket, useful as a
         * mathematical concept, even though the internal representation is stored and
         * sent using the minimum value in each bucket.
         */
        internal fun sampleToBucketIndex(sample: Long): Long {
            return log(sample.toDouble() + 1, EXPONENT).toLong()
        }

        /**
         * Determines the minimum value of a bucket, given a bucket index.
         */
        internal fun bucketIndexToBucketMinimum(bucketIndex: Long): Long {
            return pow(EXPONENT, bucketIndex.toDouble()).toLong()
        }

        /**
         * Maps a sample to the minimum value of the bucket it belongs in.
         */
        internal fun sampleToBucketMinimum(sample: Long): Long {
            return if (sample == 0L) {
                0L
            } else {
                bucketIndexToBucketMinimum(sampleToBucketIndex(sample))
            }
        }

        /**
         * Factory function that takes stringified JSON and converts it back into a
         * [TimingDistributionData].  This tries to read all values and attempts to
         * use a default where no value exists.
         *
         * @param json Stringified JSON value representing a [TimingDistributionData] object
         * @return A [TimingDistributionData] or null if unable to rebuild from the string.
         */
        @Suppress("ReturnCount", "ComplexMethod", "NestedBlockDepth")
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
            // Attempt to parse the values map, if it fails then something is wrong and we need to
            // return null.
            val values = try {
                val mapData = jsonObject.getJSONObject("values")
                val valueMap: MutableMap<Long, Long> = mutableMapOf()
                mapData.keys().forEach { key ->
                    mapData.tryGetLong(key)?.let {
                        // Don't restore buckets with zero values. They are unnecessary,
                        // and it also makes it easier to determine the contiguous range of
                        // buckets that we need to fill in the ping when we send it out if
                        // we can assume the values map never as 0 values.
                        if (it != 0L) {
                            valueMap[key.toLong()] = it
                        }
                    }
                }
                valueMap
            } catch (e: org.json.JSONException) {
                // This should only occur if there isn't a key/value pair stored for "values"
                return null
            }
            val sum = jsonObject.tryGetLong("sum") ?: return null

            return TimingDistributionData(
                category = category,
                name = name,
                values = values,
                sum = sum
            )
        }
    }

    // This is a calculated read-only property that returns the total count of accumulated values
    val count: Long
        get() = values.map { it.value }.sum()

    // This is a helper property to build the correct identifier for the metric and allow for
    // blank categories
    internal val identifier: String = if (category.isEmpty()) { name } else { "$category.$name" }

    /**
     * Accumulates a sample to the correct bucket.
     * If a value doesn't exist for this bucket yet, one is created.
     *
     * @param sample Long value representing the sample that is being accumulated
     */
    internal fun accumulate(sample: Long) {
        var bucketMinimum = sampleToBucketMinimum(sample)
        values[bucketMinimum] = (values[bucketMinimum] ?: 0) + 1
        sum += sample
    }

    /**
     * Helper function to build the [TimingDistributionData] into a JSONObject for serialization
     * purposes.
     */
    internal fun toJsonObject(): JSONObject {
        val completeValues = if (values.size != 0) {
            // A bucket range is defined by its own key, and the key of the next
            // highest bucket. This emplicitly adds any empty buckets (even if they have values
            // of 0) between the lowest and highest bucket so that the backend knows the
            // bucket ranges even without needing to know that function that was used to
            // create the buckets.
            val minBucket = sampleToBucketIndex(values.keys.min()!!)
            val maxBucket = sampleToBucketIndex(values.keys.max()!!) + 1

            var completeValues: MutableMap<String, Long> = mutableMapOf()

            for (i in minBucket..maxBucket) {
                val bucketMinimum = bucketIndexToBucketMinimum(i)
                val bucketSum = values.get(bucketMinimum)?.let { it } ?: 0
                completeValues[bucketMinimum.toString()] = bucketSum
            }

            completeValues
        } else {
            values
        }

        return JSONObject(mapOf(
            "category" to category,
            "name" to name,
            "values" to completeValues,
            "sum" to sum
        ))
    }
}
