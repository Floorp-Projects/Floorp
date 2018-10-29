/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.storages

import android.support.annotation.VisibleForTesting
import org.json.JSONObject

/**
 * This singleton handles the in-memory storage logic for strings. It is meant to be used by
 * the Specific Strings API and the ping assembling objects. No validation on the stored data
 * is performed at this point: validation must be performed by the Specific Strings API.
 */
internal object StringsStorageEngine : StorageEngine {
    private val stringStores: MutableMap<String, MutableMap<String, String>> = mutableMapOf()

    /**
     * Record a string in the desired stores.
     *
     * @param stores the list of stores to record the string into
     * @param category the category of the string
     * @param name the name of the string
     * @param value the string value to record
     */
    public fun record(
        stores: List<String>,
        category: String,
        name: String,
        value: String
    ) {
        // Record a copy of the string in all the needed stores.
        synchronized(this) {
            for (storeName in stores) {
                val storeData = stringStores.getOrPut(storeName) { mutableMapOf() }
                storeData.put("$category.$name", value)
            }
        }
    }

    /**
     * Retrieves the [recorded string data][String] for the provided
     * store name.
     *
     * @param storeName the name of the desired string store
     * @param clearStore whether or not to clearStore the requested string store
     *
     * @return the strings recorded in the requested store
     */
    @Synchronized
    public fun getSnapshot(storeName: String, clearStore: Boolean): MutableMap<String, String>? {
        if (clearStore) {
            return stringStores.remove(storeName)
        }

        return stringStores.get(storeName)
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
        return getSnapshot(storeName, clearStore)?.let { stringMap ->
            return JSONObject(stringMap)
        }
    }

    @VisibleForTesting
    internal fun clearAllStores() {
        stringStores.clear()
    }
}
