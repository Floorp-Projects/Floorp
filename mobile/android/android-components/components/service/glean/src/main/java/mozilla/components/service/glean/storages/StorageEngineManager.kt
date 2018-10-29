/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.storages

import org.json.JSONObject

/**
 * This singleton is the one interface to all the available storage engines:
 * it provides a convenient way to collect the data stored in a particular store
 * and serialize it
 */
internal class StorageEngineManager(
    private val storageEngines: Map<String, StorageEngine> = mapOf(
        "events" to EventsStorageEngine,
        "strings" to StringsStorageEngine
    )
) {
    /**
     * Collect the recorded data for the requested storage.
     *
     * @param storeName the name of the storage of interest
     * @return a [JSONObject] containing the data collected from all the
     *         storage engines.
     */
    fun collect(storeName: String): JSONObject {
        val jsonPing = JSONObject()

        for ((sectionName, engine) in storageEngines) {
            val engineData = engine.getSnapshotAsJSON(storeName, clearStore = true)
            jsonPing.put(sectionName, engineData)
        }

        return jsonPing
    }
}
