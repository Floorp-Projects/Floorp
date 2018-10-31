/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.storages

import java.util.UUID

import android.support.annotation.VisibleForTesting

/**
 * This singleton handles the in-memory storage logic for uuids. It is meant to be used by
 * the Specific UUID API and the ping assembling objects. No validation on the stored data
 * is performed at this point: validation must be performed by the Specific Uuids API.
 */
internal object UuidsStorageEngine {
    private val uuidStores: MutableMap<String, MutableMap<String, UUID>> = mutableMapOf()

    /**
     * Record a uuid in the desired stores.
     *
     * @param stores the list of stores to record the uuid into
     * @param category the category of the uuid
     * @param name the name of the uuid
     * @param value the uuid value to record
     */
    public fun record(
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
     * Retrieves the [recorded uuid data][Uuid] for the provided
     * store name.
     *
     * @param storeName the name of the desired uuid store
     * @param clearStore whether or not to clearStore the requested uuid store
     *
     * @return the uuids recorded in the requested store
     */
    @Synchronized
    public fun getSnapshot(storeName: String, clearStore: Boolean): MutableMap<String, UUID>? {
        if (clearStore) {
            return uuidStores.remove(storeName)
        }

        return uuidStores.get(storeName)
    }

    @VisibleForTesting
    internal fun clearAllStores() {
        uuidStores.clear()
    }
}
