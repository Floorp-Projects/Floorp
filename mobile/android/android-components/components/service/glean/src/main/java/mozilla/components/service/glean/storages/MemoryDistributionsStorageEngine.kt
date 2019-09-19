/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.storages

import android.content.SharedPreferences
import mozilla.components.service.glean.error.ErrorRecording
import mozilla.components.service.glean.histogram.FunctionalHistogram
import mozilla.components.service.glean.private.CommonMetricData
import mozilla.components.service.glean.private.MemoryUnit
import mozilla.components.service.glean.utils.memoryToBytes

import mozilla.components.support.base.log.logger.Logger
import org.json.JSONObject

/**
 * This singleton handles the in-memory storage logic for memory distributions. It is meant to be
 * used by the Memory Distribution API and the ping assembling objects.
 */
internal object MemoryDistributionsStorageEngine : MemoryDistributionsStorageEngineImplementation()

internal open class MemoryDistributionsStorageEngineImplementation(
    override val logger: Logger = Logger("glean/MemoryDistributionsStorageEngine")
) : GenericStorageEngine<FunctionalHistogram>() {

    companion object {
        // The base of the logarithm used to determine bucketing
        internal const val LOG_BASE = 2.0

        // The buckets per each order of magnitude of the logarithm.
        internal const val BUCKETS_PER_MAGNITUDE = 16.0

        // Set a maximum recordable value of 1 terabyte so the buckets aren't
        // completely unbounded.
        internal const val MAX_BYTES: Long = 1L shl 40
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
     * Samples greater than 1TB are truncated to 1TB.
     *
     * @param metricData the metric information for the memory distribution
     * @param sample the value to accumulate
     * @param memoryUnit the unit of the sample
     */
    @Synchronized
    fun accumulate(
        metricData: CommonMetricData,
        sample: Long,
        memoryUnit: MemoryUnit
    ) {
        accumulateSamples(metricData, longArrayOf(sample), memoryUnit)
    }

    /**
     * Accumulate an array of samples for the provided metric.
     *
     * Samples greater than 1TB are truncated to 1TB.
     *
     * @param metricData the metric information for the memory distribution
     * @param samples the values to accumulate, in the given `memoryUnit`
     * @param memoryUnit the unit that the given samples are in
     */
    @Suppress("ComplexMethod")
    @Synchronized
    fun accumulateSamples(
        metricData: CommonMetricData,
        samples: LongArray,
        memoryUnit: MemoryUnit
    ) {
        // Remove invalid samples, and convert to bytes
        var numTooLongSamples = 0
        var numNegativeSamples = 0
        var factor = memoryToBytes(memoryUnit, 1)
        val validSamples = samples.map { sample ->
            if (sample < 0) {
                numNegativeSamples += 1
                0
            } else {
                val sampleInBytes = sample * factor
                if (sampleInBytes > MAX_BYTES) {
                    numTooLongSamples += 1
                    MAX_BYTES
                } else {
                    sampleInBytes
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
                "Accumulate $numTooLongSamples samples longer than 1 terabyte",
                logger,
                numTooLongSamples
            )
            // Too large samples should just be truncated, but otherwise we record and handle them
        }

        val dummy = FunctionalHistogram(LOG_BASE, BUCKETS_PER_MAGNITUDE)
        validSamples.forEach { sample ->
            super.recordMetric(metricData, dummy, null) { currentValue, _ ->
                currentValue?.let {
                    it.accumulate(sample)
                    it
                } ?: let {
                    val newMD = FunctionalHistogram(LOG_BASE, BUCKETS_PER_MAGNITUDE)
                    newMD.accumulate(sample)
                    return@let newMD
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
