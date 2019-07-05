/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.storages

import android.annotation.SuppressLint
import android.content.Context
import androidx.annotation.VisibleForTesting
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.withTimeout
import mozilla.components.service.glean.Dispatchers
import mozilla.components.service.glean.Glean
import mozilla.components.service.glean.error.ErrorRecording.ErrorType
import mozilla.components.service.glean.error.ErrorRecording.recordError
import mozilla.components.service.glean.private.EventMetricType
import mozilla.components.service.glean.private.Lifetime
import mozilla.components.service.glean.utils.ensureDirectoryExists
import mozilla.components.support.base.log.logger.Logger
import org.json.JSONArray
import org.json.JSONException
import org.json.JSONObject
import java.io.File
import java.io.IOException
import kotlinx.coroutines.Dispatchers as KotlinDispatchers

/**
 * This singleton handles the in-memory storage logic for events. It is meant to be used by
 * the Specific Events API and the ping assembling objects.
 *
 * So that the data survives shutting down of the application, events are stored
 * in an append-only file on disk, in addition to the store in memory. Each line
 * of this file records a single event in JSON, exactly as it will be sent in the
 * ping.  There is one file per store.
 *
 * This class contains a reference to the Android application Context. While the IDE warns
 * us that this could leak, the application context lives as long as the application and this
 * object. For this reason, we should be safe to suppress the IDE warning.
 */
@SuppressLint("StaticFieldLeak, TooManyFunctions")
internal object EventsStorageEngine : StorageEngine {
    override lateinit var applicationContext: Context

    // Events recorded within the list should be reasonably sorted by timestamp, assuming
    // the sequence of calls to [record] has not been messed with. However, please only
    // trust the recorded timestamp values.
    internal val eventStores: MutableMap<String, MutableList<RecordedEventData>> = mutableMapOf()

    // The storage directory where the append-only events files reside.
    internal val storageDirectory: File by lazy {
        val dir = File(
            applicationContext.applicationInfo.dataDir,
            "${Glean.GLEAN_DATA_DIR}/events/"
        )
        ensureDirectoryExists(dir)
        dir
    }

    // Maximum length of any string value in the extra dictionary, in characters
    internal const val MAX_LENGTH_EXTRA_KEY_VALUE = 100

    // The position of the fields within the JSON payload for each event
    internal const val TIMESTAMP_FIELD = "timestamp"
    internal const val CATEGORY_FIELD = "category"
    internal const val NAME_FIELD = "name"
    internal const val EXTRA_FIELD = "extra"

    private val logger = Logger("glean/EventsStorageEngine")

    // Timeout for waiting on async IO (during testing only)
    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    internal const val JOB_TIMEOUT_MS = 5000L

    // A lock to prevent simultaneous writing of the event files
    private val eventFileLock = Any()

    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    var ioTask: Job? = null

    /**
     * Initialize events storage. This must be called once on application startup,
     * e.g. from [Glean.initialize], but after we are ready to send pings, since
     * this could potentially collect and send pings.
     *
     * If there are any events queued on disk, it loads them into memory so
     * that the memory and disk representations are in sync.
     *
     * Secondly, if this is the first time the application has been run since
     * rebooting, any pings containing events are assembled into pings and cleared
     * immediately, since their timestamps won't be compatible with the timestamps
     * we would create during this boot of the device.
     *
     * @param context The application context
     */
    internal fun onReadyToSendPings(@Suppress("UNUSED_PARAMETER") context: Context) {
        // We want this to run off of the main thread, because it might perform I/O.
        // However, we don't use the built-in KotlinDispatchers.IO since we need
        // to make sure this work is done before any other Glean API calls, and this
        // will force them to be queued after this work.
        @Suppress("EXPERIMENTAL_API_USAGE")
        Dispatchers.API.launch {
            // Load events from disk
            storageDirectory.listFiles()?.forEach { file ->
                val storeData = eventStores.getOrPut(file.name) { mutableListOf() }
                file.forEachLine { line ->
                    try {
                        val jsonContent = JSONObject(line)
                        val event = deserializeEvent(jsonContent)
                        storeData.add(event)
                    } catch (e: JSONException) {
                        // pass
                    }
                }
            }

            // TODO Technically, we only need to send out all pending events
            // if they came from a previous boot (since their timestamps won't match).
            // However, there are various challenges in detecting when the device has
            // been booted, so for now we just pay the penalty of a few more
            // unnecessary pings.
            if (eventStores.isNotEmpty()) {
                Glean.sendPingsByName(eventStores.keys.toList())
            }
        }
    }

    /**
     * Record an event in the desired stores.
     *
     * @param metricData the [EventMetricType] instance being recorded
     * @param monotonicElapsedMs the monotonic elapsed time since boot, in milliseconds
     * @param extra an optional, user defined String to String map used to provide richer event
     *              context if needed
     */
    fun <ExtraKeysEnum : Enum<ExtraKeysEnum>> record(
        metricData: EventMetricType<ExtraKeysEnum>,
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

        // Check that the extra content has sane values.
        val truncatedExtraKeys = extra?.toMutableMap()?.let { eventKeys ->
            for ((key, extraValue) in eventKeys) {
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

        val event = RecordedEventData(
            metricData.category,
            metricData.name,
            monotonicElapsedMs,
            truncatedExtraKeys
        )
        val jsonEvent = serializeEvent(event).toString()

        // Record a copy of the event in all the needed stores.
        synchronized(this) {
            val eventStoresToUpload: MutableList<String> = mutableListOf()
            for (storeName in metricData.sendInPings) {
                val storeData = eventStores.getOrPut(storeName) { mutableListOf() }
                storeData.add(event.copy())
                writeEventToDisk(storeName, jsonEvent)
                if (storeData.size == Glean.configuration.maxEvents) {
                    // The ping contains enough events to send now, add it to the list of pings to
                    // send
                    eventStoresToUpload.add(storeName)
                }
            }
            Glean.sendPingsByName(eventStoresToUpload)
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
        // Rewrite all of the timestamps to they are relative to the first
        // timestamp
        val events = eventStores[storeName]?.let { store ->
            if (store.size == 0) {
                logger.error("Unexpectedly got empty event store")
                null
            } else {
                val firstTimestamp = store[0].timestamp
                store.map { event ->
                    event.copy(timestamp = event.timestamp - firstTimestamp)
                }
            }
        }

        if (clearStore) {
            ioTask = GlobalScope.launch(KotlinDispatchers.IO) {
                synchronized(eventFileLock) {
                    getFile(storeName).delete()
                }
            }
            eventStores.remove(storeName)
        }

        return events
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
                eventsArray.put(serializeEvent(it))
            }
            eventsArray
        }
    }

    /**
     * Get the File object in which to store events for the given store.
     *
     * @param storeName The name of the store
     * @return File object to store events
     */
    private fun getFile(storeName: String): File {
        return File(storageDirectory, storeName)
    }

    /**
     * Serializes an event to its JSON representation.
     *
     * @param event Event data to serialize
     * @return [JSONObject] representing the event
     */
    private fun serializeEvent(event: RecordedEventData): JSONObject {
        val eventData = JSONObject(mapOf(
            TIMESTAMP_FIELD to event.timestamp,
            CATEGORY_FIELD to event.category,
            NAME_FIELD to event.name
        ))
        if (event.extra != null) {
            eventData.put(EXTRA_FIELD, JSONObject(event.extra))
        }
        return eventData
    }

    /**
     * Deserializes an event in JSON into a RecordedEventData object.
     *
     * @param jsonContent The JSONObject containing the data for the event.
     * @return [RecordedEventData] representing the event data
     */
    private fun deserializeEvent(jsonContent: JSONObject): RecordedEventData {
        val extra: Map<String, String>? = jsonContent.optJSONObject(EXTRA_FIELD)?.let {
            val extraValues: MutableMap<String, String> = mutableMapOf()
            it.names()?.let { names ->
                for (i in 0 until names.length()) {
                    extraValues[names.getString(i)] = it.getString(names.getString(i))
                }
            }
            extraValues
        }

        return RecordedEventData(
            jsonContent.getString(CATEGORY_FIELD),
            jsonContent.getString(NAME_FIELD),
            jsonContent.getLong(TIMESTAMP_FIELD),
            extra
        )
    }

    /**
     * Writes an event to a single store on disk.
     *
     * @param storeName The name of the store to add the event to.
     * @param eventContent A string of JSON as returned by [serializeEvent]
     */
    internal fun writeEventToDisk(storeName: String, eventContent: String) {
        ioTask = GlobalScope.launch(KotlinDispatchers.IO) {
            synchronized(eventFileLock) {
                try {
                    getFile(storeName).appendText("${eventContent}\n")
                } catch (e: IOException) {
                    logger.warn("IOException while writing event to disk: $e")
                }
            }
        }
    }

    override val sendAsTopLevelField: Boolean
        get() = true

    override fun clearAllStores() {
        // Wait until any writes have cleared until deleting all the files.
        // This is not a performance problem since this function is for use
        // in testing only.
        testWaitForWrites()
        synchronized(eventFileLock) {
            storageDirectory.listFiles()?.forEach {
                it.delete()
            }
        }
        eventStores.clear()
    }

    internal fun testWaitForWrites(timeout: Long = JOB_TIMEOUT_MS) {
        ioTask?.let {
            runBlocking {
                withTimeout(timeout) {
                    it.join()
                }
            }
        }
    }
}

@VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
data class RecordedEventData(
    val category: String,
    val name: String,
    var timestamp: Long,
    val extra: Map<String, String>? = null,

    internal val identifier: String = if (category.isEmpty()) { name } else { "$category.$name" }
)
