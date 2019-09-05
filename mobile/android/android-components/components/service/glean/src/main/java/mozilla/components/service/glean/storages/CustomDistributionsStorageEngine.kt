/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.storages

import android.content.SharedPreferences
import mozilla.components.service.glean.error.ErrorRecording
import mozilla.components.service.glean.histogram.PrecomputedHistogram
import mozilla.components.service.glean.private.CommonMetricData
import mozilla.components.service.glean.private.HistogramType

import mozilla.components.support.base.log.logger.Logger
import org.json.JSONObject

/**
 * This singleton handles the in-memory storage logic for custom distributions. It is meant to be
 * used by the Custom Distribution API and the ping assembling objects.
 */
internal object CustomDistributionsStorageEngine : CustomDistributionsStorageEngineImplementation()

internal open class CustomDistributionsStorageEngineImplementation(
    override val logger: Logger = Logger("glean/CustomDistributionsStorageEngine")
) : GenericStorageEngine<PrecomputedHistogram>() {

    override fun deserializeSingleMetric(metricName: String, value: Any?): PrecomputedHistogram? {
        return try {
            (value as? String)?.let {
                PrecomputedHistogram.fromJsonString(it)
            }
        } catch (e: org.json.JSONException) {
            null
        }
    }

    override fun serializeSingleMetric(
        userPreferences: SharedPreferences.Editor?,
        storeName: String,
        value: PrecomputedHistogram,
        extraSerializationData: Any?
    ) {
        val json = value.toJsonObject()
        userPreferences?.putString(storeName, json.toString())
    }

    /**
     * Accumulate an array of samples for the provided metric.
     *
     * @param metricData the metric information for the custom distribution
     * @param samples the values to accumulate
     */
    @Suppress("LongParameterList")
    @Synchronized
    fun accumulateSamples(
        metricData: CommonMetricData,
        samples: LongArray,
        rangeMin: Long,
        rangeMax: Long,
        bucketCount: Int,
        histogramType: HistogramType
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
        // PrecomputedHistogram for each value that doesn't have an existing current value.
        val dummy = PrecomputedHistogram(
            rangeMin = rangeMin,
            rangeMax = rangeMax,
            bucketCount = bucketCount,
            histogramType = histogramType
        )
        validSamples.forEach { sample ->
            super.recordMetric(metricData, dummy, null) { currentValue, _ ->
                currentValue?.let {
                    it.accumulate(sample)
                    it
                } ?: let {
                    val newTD = PrecomputedHistogram(
                        rangeMin = rangeMin,
                        rangeMax = rangeMax,
                        bucketCount = bucketCount,
                        histogramType = histogramType
                    )
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
