/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.storages

import android.annotation.SuppressLint
import android.content.Context
import android.content.SharedPreferences
import mozilla.components.service.glean.private.CommonMetricData
import mozilla.components.service.glean.private.Lifetime
import mozilla.components.support.base.log.logger.Logger
import org.json.JSONObject

/**
 * Defines an alias for a generic data storage to be used by
 * [GenericStorageEngine]. This maps a metric name to
 * its data.
 */
internal typealias GenericDataStorage<T> = MutableMap<String, T>

/**
 * Defines an alias for a generic storage map to be used by
 * [GenericStorageEngine]. This maps a store name to
 * the [GenericDataStorage] it holds.
 */
internal typealias GenericStorageMap<T> = MutableMap<String, GenericDataStorage<T>>

/**
 * Defines the typealias for the combiner function to be used by
 * [GenericStorageEngine.recordMetric] when recording new values.
 */
internal typealias MetricsCombiner<T> = (currentValue: T?, newValue: T) -> T

/**
 * A base class for common metric storage functionality. This allows sharing the common
 * store managing and lifetime behaviours.
 */
internal abstract class GenericStorageEngine<MetricType> : StorageEngine {
    override lateinit var applicationContext: Context

    // Let derived class define a logger so that they can provide a proper name,
    // useful when debugging weird behaviours.
    abstract val logger: Logger

    protected val userLifetimeStorage: SharedPreferences by lazy {
        deserializeLifetime(Lifetime.User)
    }
    protected val pingLifetimeStorage: SharedPreferences by lazy {
        deserializeLifetime(Lifetime.Ping)
    }

    // Store a map for each lifetime as an array element:
    // Array[Lifetime] = Map[StorageName, MetricType].
    protected val dataStores: Array<GenericStorageMap<MetricType>> =
        Array(Lifetime.values().size) { mutableMapOf<String, GenericDataStorage<MetricType>>() }

    /**
     * Implementor's provided function to convert deserialized 'user' lifetime
     * data to the destination [MetricType].
     *
     * @param metricName the name of the metric being deserialized
     * @param value loaded from the storage as [Any]
     *
     * @return data as [MetricType] or null if deserialization failed
     */
    protected abstract fun deserializeSingleMetric(metricName: String, value: Any?): MetricType?

    /**
     * Implementor's provided function to serialize 'user' lifetime data as needed by the data type.
     *
     * @param userPreferences [SharedPreferences.Editor] for writing preferences as needed by type.
     * @param storeName The metric store name where the data is stored in [SharedPreferences].
     * @param value The value to be stored, passed as a [MetricType] to be handled correctly by the
     *              implementor.
     * @param extraSerializationData extra data to be serialized to disk for "User" persisted values
     */
    protected abstract fun serializeSingleMetric(
        userPreferences: SharedPreferences.Editor?,
        storeName: String,
        value: MetricType,
        extraSerializationData: Any?
    )

    /**
     * Deserialize the metrics with a particular lifetime that are on disk.
     * This will be called the first time a metric is used or before a snapshot is
     * taken.
     *
     * @param lifetime a [Lifetime] to deserialize
     * @return A [SharedPreferences] reference that will be used to initialize [pingLifetimeStorage]
     *         or [userLifetimeStorage] or null if an invalid lifetime is used.
     */
    @Suppress("TooGenericExceptionCaught", "ComplexMethod")
    open fun deserializeLifetime(lifetime: Lifetime): SharedPreferences {
        require(lifetime == Lifetime.Ping || lifetime == Lifetime.User) {
            "deserializeLifetime does not support Lifetime.Application"
        }

        val prefsName = if (lifetime == Lifetime.Ping) {
            "${this.javaClass.canonicalName}.PingLifetime"
        } else {
            this.javaClass.canonicalName
        }
        val prefs = applicationContext.getSharedPreferences(prefsName, Context.MODE_PRIVATE)

        val metrics = try {
            prefs.all.entries
        } catch (e: NullPointerException) {
            // If we fail to deserialize, we can log the problem but keep on going.
            logger.error("Failed to deserialize metric with ${lifetime.name} lifetime")
            return prefs
        }

        for ((metricStoragePath, metricValue) in metrics) {
            if (!metricStoragePath.contains('#')) {
                continue
            }

            // Split the stored name in 2: we expect it to be in the format
            // store#metric.name
            val (storeName, metricName) =
                metricStoragePath.split('#', limit = 2)
            if (storeName.isEmpty()) {
                continue
            }

            val storeData = dataStores[lifetime.ordinal].getOrPut(storeName) { mutableMapOf() }
            // Only set the stored value if we're able to deserialize the persisted data.
            deserializeSingleMetric(metricName, metricValue)?.let { value ->
                storeData[metricName] = value
            } ?: logger.warn("Failed to deserialize $metricStoragePath")
        }

        return prefs
    }

    /**
     * Ensures that the lifetime metrics in [pingLifetimeStorage] and [userLifetimeStorage] is
     * loaded.  This is a no-op if they are already loaded.
     */
    private fun ensureAllLifetimesLoaded() {
        // Make sure data with the provided lifetime is loaded.
        // We still need to catch exceptions here, as `getAll()` might throw.
        @Suppress("TooGenericExceptionCaught")
        try {
            pingLifetimeStorage.all
            userLifetimeStorage.all
        } catch (e: NullPointerException) {
            // Intentionally left blank. We just want to fall through.
        }
    }

    /**
     * Retrieves the [recorded metric data][MetricType] for the provided
     * store name.
     *
     * Please note that the [Lifetime.Application] lifetime is handled implicitly
     * by never clearing its data. It will naturally clear out when restarting the
     * application.
     *
     * @param storeName the name of the desired store
     * @param clearStore whether or not to clear the requested store. Not that only
     *        metrics stored with a lifetime of [Lifetime.Ping] will be cleared.
     *
     * @return the [MetricType] recorded in the requested store
     */
    @Synchronized
    fun getSnapshot(storeName: String, clearStore: Boolean): GenericDataStorage<MetricType>? {
        val allLifetimes: GenericDataStorage<MetricType> = mutableMapOf()

        ensureAllLifetimesLoaded()

        // Get the metrics for all the supported lifetimes.
        for (store in dataStores) {
            store[storeName]?.let {
                allLifetimes.putAll(it)
            }
        }

        if (clearStore) {
            // We only allow clearing metrics with the "ping" lifetime.
            val editor = pingLifetimeStorage.edit()
            dataStores[Lifetime.Ping.ordinal][storeName]?.keys?.forEach { key ->
                editor.remove("$storeName#$key")
            }
            editor.apply()
            dataStores[Lifetime.Ping.ordinal].remove(storeName)
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
        return getSnapshot(storeName, clearStore)?.let { dataMap ->
            return JSONObject(dataMap as MutableMap<*, *>)
        }
    }

    /**
     * Return all of the metric identifiers currently holding data for the given
     * stores.
     *
     * @param stores The stores to look in.
     * @return a sequence of identifiers (including labels, if any) found in
     *     those stores.
     */
    override fun getIdentifiersInStores(stores: List<String>) = sequence {
        // Make sure data with "user" and "ping" lifetimes are loaded before getting the snapshot.
        ensureAllLifetimesLoaded()

        dataStores.forEach { lifetime ->
            stores.forEach {
                lifetime[it]?.let { store ->
                    store.forEach {
                        yield(it.key)
                    }
                }
            }
        }
    }

    /**
     * Helper function for the derived classes. It can be used to record
     * simple metrics to the internal storage. Internally, this calls [recordMetric]
     * with a custom `combine` function that only sets the new value.
     *
     * @param metricData the information about the metric
     * @param value the new value
     */
    protected fun recordMetric(
        metricData: CommonMetricData,
        value: MetricType
    ) = recordMetric(metricData, value) { _, v -> v }

    /**
     * Helper function for the derived classes. It can be used to record
     * simple metrics to the internal storage.
     *
     * @param metricData the information about the metric
     * @param value the new value
     * @param extraSerializationData extra data to be serialized to disk for "User" persisted values
     * @param combine a lambda function to combine the currently stored value and
     *        the new one; this allows to implement new behaviours such as adding.
     */
    @Synchronized
    protected fun recordMetric(
        metricData: CommonMetricData,
        value: MetricType,
        extraSerializationData: Any? = null,
        combine: MetricsCombiner<MetricType>
    ) {
        checkNotNull(applicationContext) { "No recording can take place without an application context" }

        // Record a copy of the metric in all the needed stores.
        @SuppressLint("CommitPrefEdits")
        val editor: SharedPreferences.Editor? = when (metricData.lifetime) {
            Lifetime.User -> userLifetimeStorage.edit()
            Lifetime.Ping -> pingLifetimeStorage.edit()
            else -> null
        }
        metricData.sendInPings.forEach { store ->
            val storeData = dataStores[metricData.lifetime.ordinal].getOrPut(store) { mutableMapOf() }
            // We support empty categories for enabling the internal use of metrics
            // when assembling pings in [PingMaker].
            val entryName = metricData.identifier
            val combinedValue = combine(storeData[entryName], value)
            storeData[entryName] = combinedValue
            // Persist data with "user" or "ping" lifetimes
            editor?.let {
                serializeSingleMetric(
                    it,
                    "$store#$entryName",
                    combinedValue,
                    extraSerializationData
                )
            }
        }
        editor?.apply()
    }

    override fun clearAllStores() {
        userLifetimeStorage.edit().clear().apply()
        pingLifetimeStorage.edit().clear().apply()
        dataStores.forEach { it.clear() }
    }
}
