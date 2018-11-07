/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.storages

import android.annotation.SuppressLint
import android.content.Context
import android.content.SharedPreferences
import java.util.UUID

import android.support.annotation.VisibleForTesting
import mozilla.components.service.glean.Lifetime
import mozilla.components.support.base.log.logger.Logger
import org.json.JSONObject

// Convenience aliases to make the code more readable.
internal typealias UUIDStorage = MutableMap<String, UUID>
internal typealias UUIDMap = MutableMap<String, UUIDStorage>

/**
 * This singleton handles the in-memory storage logic for uuids. It is meant to be used by
 * the Specific UUID API and the ping assembling objects. No validation on the stored data
 * is performed at this point: validation must be performed by the Specific Uuids API.
 *
 * This class contains a reference to the Android application Context. While the IDE warns
 * us that this could leak, the application context lives as long as the application and this
 * object. For this reason, we should be safe to suppress the IDE warning.
 */
@SuppressLint("StaticFieldLeak")
internal object UuidsStorageEngine : UuidsStorageEngineImplementation()

/**
 * This class implements the behaviour for UuidsStorageEngine. This is separate
 * from the object to make it easier to test it.
 */
@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
open class UuidsStorageEngineImplementation : StorageEngine {
    override lateinit var applicationContext: Context

    private val userLifetimeStorage: SharedPreferences by lazy { deserializeUserLifetime() }
    private val logger = Logger("glean/HttpPingUploader")

    // Use a multi-level map to store the data for the different lifetimes and
    // stores: Map[Lifetime, Map[StorageName, UUID]].
    private val uuidStores: Map<String, UUIDMap> =
        Lifetime.values().associateBy({ it.toString() }, { mutableMapOf<String, UUIDStorage>() })

    /**
     * Record a uuid in the desired stores.
     *
     * @param stores the list of stores to record the uuid into
     * @param category the category of the uuid
     * @param name the name of the uuid
     * @param value the uuid value to record
     */
    @Synchronized
    fun record(
        stores: List<String>,
        category: String,
        name: String,
        lifetime: Lifetime,
        value: UUID
    ) {
        checkNotNull(applicationContext) { "No recording can take place without an application context" }

        // Record a copy of the uuid in all the needed stores.
        val userPrefs: SharedPreferences.Editor? =
            if (lifetime == Lifetime.User) userLifetimeStorage.edit() else null
        for (storeName in stores) {
            val storeData = uuidStores[lifetime.toString()]!!.getOrPut(storeName) { mutableMapOf() }
            val entryName = "$category.$name"
            storeData[entryName] = value
            // Persist data with "user" lifetime
            if (lifetime == Lifetime.User) {
                userPrefs?.putString("$storeName#$entryName", value.toString())
            }
        }
        userPrefs?.apply()
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
    fun getSnapshot(storeName: String, clearStore: Boolean): UUIDStorage? {
        val allLifetimes: UUIDStorage = mutableMapOf()

        // Make sure data with "user" lifetime is loaded before getting the snapshot.
        userLifetimeStorage.all

        // Get the metrics for all the supported lifetimes.
        for ((_, store) in uuidStores) {
            store[storeName]?.let {
                allLifetimes.putAll(it)
            }
        }

        if (clearStore) {
            // We only allow clearing metrics with the "ping" lifetime.
            uuidStores[Lifetime.Ping.toString()]!!.remove(storeName)
        }

        return if (allLifetimes.isNotEmpty()) allLifetimes else null
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

    /**
     * Deserialize the metrics with a lifetime = User that are on disk.
     * This will be called the first time a metric is used or before a snapshot is
     * taken.
     *
     * @return A [SharedPreferences] reference that will be used to inititialize [userLifetimeStorage]
     */
    @Suppress("TooGenericExceptionCaught")
    private fun deserializeUserLifetime(): SharedPreferences {
        val prefs =
            applicationContext.getSharedPreferences(this.javaClass.simpleName, Context.MODE_PRIVATE)
        return try {
            for ((metricName, metricValue) in prefs.all.entries) {
                if (metricValue !is String) {
                    continue
                }

                // Split the stored name in 2: we expect it to be in the format
                // store#metric.name
                val parts = metricName.split('#', limit = 2)
                val storeData = uuidStores[Lifetime.User.toString()]!!.getOrPut(parts[0]) { mutableMapOf() }
                storeData[parts[1]] = UUID.fromString(metricValue)
            }
            prefs
        } catch (e: NullPointerException) {
            // If we fail to deserialize, we can log the problem but keep on going.
            logger.error("Failed to deserialize UUIDs with 'user' lifetime")
            prefs
        }
    }

    @VisibleForTesting
    internal fun clearAllStores() {
        for ((_, store) in uuidStores) {
            store.clear()
        }
    }
}
