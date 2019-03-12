/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.storages

import mozilla.components.service.glean.Glean
import android.annotation.SuppressLint
import android.content.Context
import android.os.SystemClock
import android.support.annotation.VisibleForTesting
import mozilla.components.service.glean.error.ErrorRecording.recordError
import mozilla.components.service.glean.error.ErrorRecording.ErrorType
import mozilla.components.service.glean.EventMetricType
import mozilla.components.service.glean.Lifetime
import mozilla.components.service.glean.scheduler.PingUploadWorker
import mozilla.components.support.base.log.logger.Logger
import org.json.JSONArray
import org.json.JSONObject

/**
 * This singleton handles the in-memory storage logic for events. It is meant to be used by
 * the Specific Events API and the ping assembling objects.
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

    // Maximum length of any string value in the extra dictionary, in characters
    internal const val MAX_LENGTH_EXTRA_KEY_VALUE = 100

    private val logger = Logger("glean/EventsStorageEngine")

    /**
     * Record an event in the desired stores.
     *
     * @param metricData the [EventMetricType] instance being recorded
     * @param monotonicElapsedMs the monotonic elapsed time since boot, in milliseconds
     * @param extra an optional, user defined String to String map used to provide richer event
     *              context if needed
     */
    fun record(
        metricData: EventMetricType,
        monotonicElapsedMs: Long,
        extra: Map<String, String>? = null
    ) {
        if (metricData.lifetime != Lifetime.Ping) {
            recordError(
                metricData,
                ErrorType.InvalidValue,
                "Must have `Ping` lifetime.",
                logger
            )
            return
        }

        // Check if the provided extra keys are allowed and have sane values.
        val truncatedExtraKeys = extra?.toMutableMap()?.let { eventKeys ->
            if (metricData.allowedExtraKeys == null) {
                recordError(
                    metricData,
                    ErrorType.InvalidValue,
                    "Cannot use extra keys when there are no extra keys defined.",
                    logger
                )
                return
            }

            for ((key, extraValue) in eventKeys) {
                if (!metricData.allowedExtraKeys.contains(key)) {
                    recordError(
                        metricData,
                        ErrorType.InvalidValue,
                        "Extra key '$key' is not allowed",
                        logger
                    )
                    return
                }

                if (extraValue.length > MAX_LENGTH_EXTRA_KEY_VALUE) {
                    recordError(
                        metricData,
                        ErrorType.InvalidValue,
                        "Extra key length ${extraValue.length} exceeds maximum of $MAX_LENGTH_EXTRA_KEY_VALUE",
                        logger
                    )
                    eventKeys[key] = extraValue.substring(0, MAX_LENGTH_EXTRA_KEY_VALUE)
                }
            }
            eventKeys
        }

        val msSinceStart = monotonicElapsedMs - startTime
        val event = RecordedEventData(metricData.category, metricData.name, msSinceStart, truncatedExtraKeys)

        // Record a copy of the event in all the needed stores.
        synchronized(this) {
            for (storeName in metricData.getStorageNames()) {
                val storeData = eventStores.getOrPut(storeName) { mutableListOf() }
                storeData.add(event.copy())
                if (storeData.size == Glean.configuration.maxEvents &&
                    Glean.assembleAndSerializePing(storeName)) {
                    // Queue background worker to upload the newly created ping
                    PingUploadWorker.enqueueWorker()
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
                if (it.extra != null) {
                    eventData.put(JSONObject(it.extra))
                }
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
    val msSinceStart: Long,
    val extra: Map<String, String>? = null,

    internal val identifier: String = if (category.isEmpty()) { name } else { "$category.$name" }
)
