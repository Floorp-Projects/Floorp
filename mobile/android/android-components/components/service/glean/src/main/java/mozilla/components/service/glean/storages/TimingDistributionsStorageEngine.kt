/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.storages

import android.content.SharedPreferences
import mozilla.components.service.glean.error.ErrorRecording
import mozilla.components.service.glean.histogram.FunctionalHistogram
import mozilla.components.service.glean.private.CommonMetricData
import mozilla.components.service.glean.private.TimeUnit
import mozilla.components.service.glean.utils.timeToNanos

import mozilla.components.support.base.log.logger.Logger
import org.json.JSONObject

/**
 * This singleton handles the in-memory storage logic for timing distributions. It is meant to be
 * used by the Timing Distribution API and the ping assembling objects.
 */
internal object TimingDistributionsStorageEngine : TimingDistributionsStorageEngineImplementation()

internal open class TimingDistributionsStorageEngineImplementation(
    override val logger: Logger = Logger("glean/TimingDistributionsStorageEngine")
) : GenericStorageEngine<FunctionalHistogram>() {

    companion object {
        // The base of the logarithm used to determine bucketing
        internal const val LOG_BASE = 2.0

        // The buckets per each order of magnitude of the logarithm.
        internal const val BUCKETS_PER_MAGNITUDE = 8.0

        // Maximum time of 10 minutes in nanoseconds. This maximum means we
        // retain a maximum of 313 buckets.
        internal const val MAX_SAMPLE_TIME: Long = 1000L * 1000L * 1000L * 60L * 10L
    }

    override fun deserializeSingleMetric(metricName: String, value: Any?): FunctionalHistogram? {
        return try {
            (value as? String)?.let {
                FunctionalHistogram.fromJsonString(it)
            }
        } catch (e: org.json.JSONException) {
            null
        }
    }

    override fun serializeSingleMetric(
        userPreferences: SharedPreferences.Editor?,
        storeName: String,
        value: FunctionalHistogram,
        extraSerializationData: Any?
    ) {
        val json = value.toJsonObject()
        userPreferences?.putString(storeName, json.toString())
    }

    /**
     * Accumulate value for the provided metric.
     *
     * Samples greater than 10 minutes in length are truncated to 10 minutes.
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
     * Samples greater than 10 minutes in length are truncated to 10 minutes.
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
            // Too long samples should just be truncated, but otherwise we record and handle them
        }

        val dummy = FunctionalHistogram(LOG_BASE, BUCKETS_PER_MAGNITUDE)
        validSamples.forEach { sample ->
            super.recordMetric(metricData, dummy, null) { currentValue, _ ->
                currentValue?.let {
                    it.accumulate(sample)
                    it
                } ?: let {
                    val newTD = FunctionalHistogram(LOG_BASE, BUCKETS_PER_MAGNITUDE)
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
                jsonObj.put(it.key, it.value.toJsonPayloadObject())
            }
            return jsonObj
        }
    }
}
