/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.storages

import mozilla.components.service.glean.Glean
import android.annotation.SuppressLint
import android.content.Context
import android.os.SystemClock
import android.support.annotation.VisibleForTesting
import org.json.JSONArray

/**
 * This singleton handles the in-memory storage logic for events. It is meant to be used by
 * the Specific Events API and the ping assembling objects. No validation on the stored data
 * is performed at this point: validation must be performed by the Specific Events API.
 *
 * This class contains a reference to the Android application Context. While the IDE warns
 * us that this could leak, the application context lives as long as the application and this
 * object. For this reason, we should be safe to suppress the IDE warning.
 */
@SuppressLint("StaticFieldLeak")
internal object EventsStorageEngine : StorageEngine {
    override lateinit var applicationContext: Context

    // Events recorded within the list should be reasonably sorted by timestamp, assuming
    // the sequence of calls to [record] has not been messed with. However, please only
    // trust the recorded timestamp values.
    private val eventStores: MutableMap<String, MutableList<RecordedEventData>> = mutableMapOf()

    // The timestamp of recorded events is computed relative to this point in time
    // which should roughly match with the process start time. Note that, according
    // to the docs, the used clock is guaranteed to be monotonic.
    private val startTime: Long = SystemClock.elapsedRealtime()

    /**
     * Record an event in the desired stores.
     *
     * @param stores the list of stores to record the event into
     * @param category the category of the event
     * @param name the name of the event
     * @param objectId the id of the event object
     * @param value an optional, user defined value providing context for the event
     * @param extra an optional, user defined String to String map used to provide richer event
     *              context if needed
     */
    @Suppress("LongParameterList")
    fun record(
        stores: List<String>,
        category: String,
        name: String,
        objectId: String,
        value: String? = null,
        extra: Map<String, String>? = null
    ) {
        val msSinceStart = SystemClock.elapsedRealtime() - startTime
        val event = RecordedEventData(category, name, objectId, msSinceStart, value, extra)

        // Record a copy of the event in all the needed stores.
        synchronized(this) {
            for (storeName in stores) {
                val storeData = eventStores.getOrPut(storeName) { mutableListOf() }
                storeData.add(event.copy())
                if (storeData.size == Glean.configuration.maxEvents) {
                    Glean.sendPing(storeName, "events")
                }
            }
        }
    }

    /**
     * Retrieves the [recorded event data][RecordedEventData] for the provided
     * store name.
     *
     * @param storeName the name of the desired event store
     * @param clearStore whether or not to clearStore the requested event store
     *
     * @return the list of events recorded in the requested store
     */
    @Synchronized
    fun getSnapshot(storeName: String, clearStore: Boolean): List<RecordedEventData>? {
        if (clearStore) {
            return eventStores.remove(storeName)
        }

        return eventStores[storeName]
    }

    /**
     * Get a snapshot of the stored data as a JSON object.
     *
     * @param storeName the name of the desired store
     * @param clearStore whether or not to clearStore the requested store
     *
     * @return the JSONArray containing the recorded data
     */
    override fun getSnapshotAsJSON(storeName: String, clearStore: Boolean): Any? {
        return getSnapshot(storeName, clearStore)?.let { pingEvents ->
            val eventsArray = JSONArray()
            pingEvents.forEach {
                val eventData = JSONArray()
                eventData.put(it.msSinceStart)
                eventData.put(it.category)
                eventData.put(it.name)
                eventData.put(it.objectId)
                eventData.put(it.value)
                eventData.put(it.extra)
                eventsArray.put(eventData)
            }
            return eventsArray
        }
    }

    override val sendAsTopLevelField: Boolean
        get() = true

    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    override fun clearAllStores() {
        eventStores.clear()
    }
}

@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
data class RecordedEventData(
    val category: String,
    val name: String,
    val objectId: String,
    val msSinceStart: Long,
    val value: String? = null,
    val extra: Map<String, String>? = null,

    internal val identifier: String = if (category.isEmpty()) { name } else { "$category.$name" }
)
