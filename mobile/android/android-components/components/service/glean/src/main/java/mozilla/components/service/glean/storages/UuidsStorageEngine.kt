/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.storages

import java.util.UUID

import android.support.annotation.VisibleForTesting
import org.json.JSONObject

/**
 * This singleton handles the in-memory storage logic for uuids. It is meant to be used by
 * the Specific UUID API and the ping assembling objects. No validation on the stored data
 * is performed at this point: validation must be performed by the Specific Uuids API.
 */
internal object UuidsStorageEngine : StorageEngine  {

    private val uuidStores: MutableMap<String, MutableMap<String, UUID>> = mutableMapOf()

    /**
     * Record a uuid in the desired stores.
     *
     * @param stores the list of stores to record the uuid into
     * @param category the category of the uuid
     * @param name the name of the uuid
     * @param value the uuid value to record
     */
    fun record(
        stores: List<String>,
        category: String,
        name: String,
        value: UUID
    ) {
        // Record a copy of the uuid in all the needed stores.
        synchronized(this) {
            for (storeName in stores) {
                val storeData = uuidStores.getOrPut(storeName) { mutableMapOf() }
                storeData.put("$category.$name", value)
            }
        }
    }

    /**
     * Retrieves the [recorded uuid data][UUID] for the provided
     * store name.
     *
     * @param storeName the name of the desired uuid store
     * @param clearStore whether or not to clearStore the requested uuid store
     *
     * @return the uuids recorded in the requested store
     */
    @Synchronized
    fun getSnapshot(storeName: String, clearStore: Boolean): MutableMap<String, UUID>? {
        if (clearStore) {
            return uuidStores.remove(storeName)
        }

        return uuidStores.get(storeName)
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
        return getSnapshot(storeName, clearStore)?.let { uuidMap ->
            return JSONObject(uuidMap)
        }
    }

    @VisibleForTesting
    internal fun clearAllStores() {
        uuidStores.clear()
    }
}
