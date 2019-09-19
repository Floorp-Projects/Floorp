/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.storages

import android.annotation.SuppressLint
import android.content.SharedPreferences
import mozilla.components.service.glean.error.ErrorRecording
import mozilla.components.service.glean.private.CommonMetricData
import mozilla.components.service.glean.private.TimeUnit
import mozilla.components.service.glean.utils.getAdjustedTime

import mozilla.components.support.base.log.logger.Logger
import org.json.JSONArray
import org.json.JSONObject

/**
 * This singleton handles the in-memory storage logic for timespans. It is meant to be used by
 * the Specific Timespan API and the ping assembling objects.
 *
 * This class contains a reference to the Android application Context. While the IDE warns
 * us that this could leak, the application context lives as long as the application and this
 * object. For this reason, we should be safe to suppress the IDE warning.
 */
@SuppressLint("StaticFieldLeak")
internal object TimespansStorageEngine : TimespansStorageEngineImplementation()

internal open class TimespansStorageEngineImplementation(
    override val logger: Logger = Logger("glean/TimespansStorageEngine")
) : GenericStorageEngine<Long>() {

    /**
     * An internal map to keep track of the desired time units for the recorded timespans.
     * We need this in order to get a snapshot of the data, with the right time unit,
     * later on.
     */
    private val timeUnitsMap = mutableMapOf<String, TimeUnit>()

    override fun deserializeSingleMetric(metricName: String, value: Any?): Long? {
        val jsonArray = (value as? String)?.let {
            return@let try {
                JSONArray(it)
            } catch (e: org.json.JSONException) {
                null
            }
        }

        // In order to perform timeunit conversion when taking a snapshot, we persisted
        // the desired time unit together with the raw values. We unpersist the first element
        // in the array as the time unit, the second as the raw Long value.
        if (jsonArray == null || jsonArray.length() != 2) {
            logger.error("Unexpected format found when deserializing $metricName")
            return null
        }

        return try {
            val timeUnit = jsonArray.getInt(0)
            val rawValue = jsonArray.getLong(1)
            // If nothing threw, make sure our time unit is within the enum's range
            // and finally set/return the values.
            TimeUnit.values().getOrNull(timeUnit)?.let {
                timeUnitsMap[metricName] = it
                rawValue
            }
        } catch (e: org.json.JSONException) {
            null
        }
    }

    override fun serializeSingleMetric(
        userPreferences: SharedPreferences.Editor?,
        storeName: String,
        value: Long,
        extraSerializationData: Any?
    ) {
        // To support converting to the desired time unit when taking a snapshot, we need a way
        // to know the time unit for timespans that are loaded off the disk, for user lifetime.
        // To do that, instead of simply persisting a Long, we instead persist a JSONArray. The
        // first item in this array is the the time unit, the second is the long value.

        // We expect to have received the time unit as extraSerializationData. There's
        // no point in persisting if we didn't.
        if (extraSerializationData == null ||
            extraSerializationData !is TimeUnit) {
            logger.error("Unexpected or missing extra data for time unit serialization")
            return
        }

        val tuple = JSONArray()
        tuple.put(extraSerializationData.ordinal)
        tuple.put(value)
        userPreferences?.putString(storeName, tuple.toString())
    }

    /**
     * Set the elapsed time explicitly.
     *
     * @param metricData the metric information for the timespan
     * @param timeUnit the time unit we want the data in when snapshotting
     * @param elapsedNanos the time to record, in nanoseconds
     */
    @Synchronized
    fun set(
        metricData: CommonMetricData,
        timeUnit: TimeUnit,
        elapsedNanos: Long
    ) {
        // Look for the start time: if it's there, commit the timespan.
        val timespanName = metricData.identifier

        // Store the time unit: we'll need it when snapshotting.
        timeUnitsMap[timespanName] = timeUnit

        super.recordMetric(metricData, elapsedNanos, timeUnit) { oldValue, newValue ->
            oldValue?.let {
                // Report an error if we attempt to set a value and we already
                // have one.
                ErrorRecording.recordError(
                    metricData,
                    ErrorRecording.ErrorType.InvalidValue,
                    "Timespan value already recorded. New value discarded.",
                    logger
                )
                // Do not overwrite the old value.
                it
            } ?: newValue
        }
    }

    /**
     * Get a snapshot of the stored timespans and adjust it to the desired time units.
     *
     * @param storeName the name of the desired store
     * @param clearStore whether or not to clear the requested store. Not that only
     *        metrics stored with a lifetime of [Lifetime.Ping] will be cleared.
     *
     * @return the [Long] recorded in the requested store
     */
    @Synchronized
    internal fun getSnapshotWithTimeUnit(storeName: String, clearStore: Boolean): Map<String, Pair<String, Long>>? {
        val adjustedData = super.getSnapshot(storeName, clearStore)
            ?.mapValuesTo(mutableMapOf<String, Pair<String, Long>>()) {
            // Convert to the expected time unit.
            if (it.key !in timeUnitsMap) {
                logger.error("Can't find the time unit for ${it.key}. Reporting raw value.")
            }

            timeUnitsMap[it.key]?.let { timeUnit ->
                Pair(timeUnit.name.toLowerCase(), getAdjustedTime(timeUnit, it.value))
            } ?: Pair("unknown", it.value)
        }

        // Clear the time unit map if needed: we need to check all the stores
        // for all the lifetimes.
        if (clearStore) {
            // Get a list of the metrics that are still stored. We'll drop the time units for all the
            // metrics that are not in this set.
            val unclearedMetricNames =
                dataStores.flatMap { lifetime -> lifetime.entries }.flatMap { it -> it.value.keys }.toSet()

            timeUnitsMap.keys.retainAll { it in unclearedMetricNames }
        }

        return adjustedData
    }

    /**
     * Get a snapshot of the stored data as a JSON object, including
     * the time_unit for each field.
     *
     * @param storeName the name of the desired store
     * @param clearStore whether or not to clearStore the requested store
     *
     * @return the [JSONObject] containing the recorded data.
     */
    override fun getSnapshotAsJSON(storeName: String, clearStore: Boolean): Any? {
        return getSnapshotWithTimeUnit(storeName, clearStore)?.let { dataMap ->
            val data = dataMap.mapValuesTo(mutableMapOf()) {
                JSONObject(mapOf(
                    "time_unit" to it.value.first,
                    "value" to it.value.second
                ))
            }
            return JSONObject(data as MutableMap<*, *>)
        }
    }

    /**
     * Test-only method used to clear the timespans stores.
     */
    override fun clearAllStores() {
        super.clearAllStores()
        timeUnitsMap.clear()
    }
}
