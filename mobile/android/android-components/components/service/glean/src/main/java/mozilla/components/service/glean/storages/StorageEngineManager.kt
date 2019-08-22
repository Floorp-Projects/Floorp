/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.storages

import android.content.Context
import mozilla.components.service.glean.private.BooleanMetricType
import mozilla.components.service.glean.private.CounterMetricType
import mozilla.components.service.glean.private.CustomDistributionMetricType
import mozilla.components.service.glean.private.DatetimeMetricType
import mozilla.components.service.glean.private.MemoryDistributionMetricType
import mozilla.components.service.glean.private.QuantityMetricType
import mozilla.components.service.glean.private.StringListMetricType
import mozilla.components.service.glean.private.StringMetricType
import mozilla.components.service.glean.private.TimespanMetricType
import mozilla.components.service.glean.private.TimingDistributionMetricType
import mozilla.components.service.glean.private.UuidMetricType
import mozilla.components.support.ktx.android.org.json.getOrPutJSONObject
import org.json.JSONArray
import org.json.JSONObject

/**
 * This singleton is the one interface to all the available storage engines:
 * it provides a convenient way to collect the data stored in a particular store
 * and serialize it
 */
internal class StorageEngineManager(
    private val storageEngines: Map<String, StorageEngine> = mapOf(
        "boolean" to BooleansStorageEngine,
        "counter" to CountersStorageEngine,
        "custom_distribution" to CustomDistributionsStorageEngine,
        "datetime" to DatetimesStorageEngine,
        "events" to EventsStorageEngine,
        "memory_distribution" to MemoryDistributionsStorageEngine,
        "quantity" to QuantitiesStorageEngine,
        "string" to StringsStorageEngine,
        "string_list" to StringListsStorageEngine,
        "timespan" to TimespansStorageEngine,
        "timing_distribution" to TimingDistributionsStorageEngine,
        "uuid" to UuidsStorageEngine
    ),
    applicationContext: Context
) {
    init {
        for ((_, engine) in storageEngines) {
            engine.applicationContext = applicationContext
        }
    }

    /**
     * Splits a labeled metric back into its name/label parts.
     *
     * If not a labeled metric, the second part of the Pair will be null.
     *
     * @param key The key for the metric value in the flattened storage engine
     * @return A pair (metricName, label).  label is null if key is not
     * from a labeled metric.
     */
    private fun parseLabeledMetric(key: String): Pair<String, String?> {
        val divider = key.indexOf('/', 1)
        if (divider >= 0) {
            return Pair(
                key.substring(0, divider),
                key.substring(divider + 1)
            )
        } else {
            return Pair(key, null)
        }
    }

    /**
     * Reorganizes the flat storage of metrics into labeled and unlabeled categories as
     * they appear in the ping.
     *
     * The unlabeled metrics go under the `sectionName` key, and the labeled metrics go
     * under the `labeled_$sectionName` key.
     *
     * @param sectionName The name of the metric section (the name of the metric type)
     * @param dst The destination JSONObject in the ping
     * @param engineData The flat object of metrics from the storage engine
     */
    private fun separateLabeledAndUnlabeledMetrics(
        sectionName: String,
        dst: JSONObject,
        engineData: JSONObject
    ) {
        for (key in engineData.keys()) {
            val parts = parseLabeledMetric(key)
            parts.second?.let {
                val labeledSection = dst.getOrPutJSONObject("labeled_$sectionName") { JSONObject() }
                val labeledMetric = labeledSection.getOrPutJSONObject(parts.first) { JSONObject() }
                labeledMetric.put(it, engineData.get(key))
            } ?: run {
                val section = dst.getOrPutJSONObject(sectionName) { JSONObject() }
                section.put(key, engineData.get(key))
            }
        }
    }

    /**
     * Collect the recorded data for the requested storage.
     *
     * @param storeName the name of the storage of interest
     * @return a [JSONObject] containing the data collected from all the
     *         storage engines or returns a completely empty, zero-length [JSONObject]
     *         if the store is empty and there are no real metrics to send.
     */
    fun collect(storeName: String): JSONObject {
        val jsonPing = JSONObject()
        val metricsSection = JSONObject()
        for ((sectionName, engine) in storageEngines) {
            val engineData = engine.getSnapshotAsJSON(storeName, clearStore = true)
            val dst = if (engine.sendAsTopLevelField) jsonPing else metricsSection
            // Most storage engines return a JSONObject mapping metric names
            // to metric values, and these can include labeled metrics that we
            // need to separate out.  The EventsStorageEngine just returns an
            // array of events, which are never "labeled".
            if (engineData is JSONObject) {
                separateLabeledAndUnlabeledMetrics(sectionName, dst, engineData)
            } else if (engineData is JSONArray) {
                dst.put(sectionName, engineData)
            }
        }
        if (metricsSection.length() != 0) {
            jsonPing.put("metrics", metricsSection)
        }

        return jsonPing
    }

    /**
     * Clear all recorded metrics in all stores.
     */
    internal fun clearAllStores() {
        for (storageEngine in storageEngines) {
            storageEngine.value.clearAllStores()
        }
    }

    companion object {
        /**
        * Get the storage engine associated with a given metric type.
        *
        * @return A storage engine, or null if none exists
        */
        internal fun <T> getStorageEngineForMetric(subMetric: T): StorageEngine? {
            // Every metric that supports labels needs an entry here
            return when (subMetric) {
                is BooleanMetricType -> BooleansStorageEngine
                is CounterMetricType -> CountersStorageEngine
                is CustomDistributionMetricType -> CustomDistributionsStorageEngine
                is DatetimeMetricType -> DatetimesStorageEngine
                is MemoryDistributionMetricType -> MemoryDistributionsStorageEngine
                is QuantityMetricType -> QuantitiesStorageEngine
                is StringListMetricType -> StringListsStorageEngine
                is StringMetricType -> StringsStorageEngine
                is TimingDistributionMetricType -> TimingDistributionsStorageEngine
                is TimespanMetricType -> TimespansStorageEngine
                is UuidMetricType -> UuidsStorageEngine
                else -> null
            }
        }
    }
}
