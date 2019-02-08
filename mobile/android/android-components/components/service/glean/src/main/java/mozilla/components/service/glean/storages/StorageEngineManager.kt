/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.storages

import android.content.Context
import android.support.annotation.VisibleForTesting
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
        "events" to EventsStorageEngine,
        "string" to StringsStorageEngine,
        "string_list" to StringListsStorageEngine,
        "timespan" to TimespansStorageEngine,
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
            if (engine.sendAsTopLevelField) {
                jsonPing.put(sectionName, engineData)
            } else {
                metricsSection.put(sectionName, engineData)
            }
        }
        if (metricsSection.length() != 0) {
            jsonPing.put("metrics", metricsSection)
        }

        return jsonPing
    }

    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    fun clearAllStores() {
        for (storageEngine in storageEngines) {
            storageEngine.value.clearAllStores()
        }
    }
}
