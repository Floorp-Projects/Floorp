/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.ping

import android.content.Context
import android.content.SharedPreferences
import androidx.annotation.VisibleForTesting
import mozilla.components.service.glean.BuildConfig
import mozilla.components.service.glean.private.PingType
import mozilla.components.service.glean.storages.StorageEngineManager
import mozilla.components.service.glean.storages.ExperimentsStorageEngine
import mozilla.components.service.glean.utils.getISOTimeString
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.android.org.json.mergeWith
import org.json.JSONException
import org.json.JSONObject

internal class PingMaker(
    private val storageManager: StorageEngineManager,
    private val applicationContext: Context
) {
    private val logger = Logger("glean/PingMaker")
    private val pingStartTimes: MutableMap<String, String> = mutableMapOf()
    private val objectStartTime = getISOTimeString()
    internal val sharedPreferences: SharedPreferences? by lazy {
        applicationContext.getSharedPreferences(
            this.javaClass.canonicalName,
            Context.MODE_PRIVATE
        )
    }

    /**
     * Get the ping sequence number for a given ping. This is a
     * monotonically-increasing value that is updated every time a particular ping
     * type is sent.
     *
     * @param pingName The name of the ping
     * @return sequence number
     */
    internal fun getPingSeq(pingName: String): Int {
        sharedPreferences?.let {
            val key = "${pingName}_seq"
            val currentValue = it.getInt(key, 0)
            val editor = it.edit()
            editor.putInt(key, currentValue + 1)
            editor.apply()
            return currentValue
        }

        // This clause should happen in testing only, where a sharedPreferences object
        // isn't guaranteed to exist if using a mocked ApplicationContext
        logger.error("Couldn't get SharedPreferences object for ping sequence number")
        return 0
    }

    /**
     * Reset all ping sequence numbers.
     */
    internal fun resetPingSequenceNumbers() {
        sharedPreferences?.let {
            it.edit().clear().apply()
        }
    }

    /**
     * Return the object containing the "ping_info" section of a ping.
     *
     * @param pingName the name of the ping to be sent
     * @return a [JSONObject] containing the "ping_info" data
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    fun getPingInfo(pingName: String): JSONObject {
        val pingInfo = JSONObject()
        pingInfo.put("ping_type", pingName)

        // Experiments belong in ping_info, because they must appear in every ping
        pingInfo.put("experiments", ExperimentsStorageEngine.getSnapshotAsJSON("", false))

        pingInfo.put("seq", getPingSeq(pingName))

        // This needs to be a bit more involved for start-end times. "start_time" is
        // the time the ping was generated the last time. If not available, we use the
        // date the object was initialized.
        val startTime = if (pingName in pingStartTimes) pingStartTimes[pingName] else objectStartTime
        pingInfo.put("start_time", startTime)
        val endTime = getISOTimeString()
        pingInfo.put("end_time", endTime)
        // Update the start time with the current time.
        pingStartTimes[pingName] = endTime
        return pingInfo
    }

    /**
     * Return the object containing the "client_info" section of a ping.
     *
     * @param includeClientId When `true`, include the "client_id" metric.
     * @return a [JSONObject] containing the "client_info" data
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    fun getClientInfo(includeClientId: Boolean): JSONObject {
        val clientInfo = JSONObject()
        clientInfo.put("telemetry_sdk_build", BuildConfig.LIBRARY_VERSION)
        clientInfo.mergeWith(getClientInfoMetrics(includeClientId))
        return clientInfo
    }

    /**
     * Collect the metrics stored in the "glean_client_info" bucket.
     *
     * @param includeClientId When `true`, include the "client_id" metric.
     * @return a [JSONObject] containing the metrics belonging to the "client_info"
     *         section of the ping.
     */
    private fun getClientInfoMetrics(includeClientId: Boolean): JSONObject {
        val pingInfoData = storageManager.collect("glean_client_info")

        // The data returned by the manager is keyed by the storage engine name.
        // For example, the client id will live in the "uuid" object, within
        // `clientInfoData`. Remove the first level of indirection and return
        // the flattened data to the caller.
        val flattenedData = JSONObject()
        try {
            val metricsData = pingInfoData.getJSONObject("metrics")
            for (key in metricsData.keys()) {
                flattenedData.mergeWith(metricsData.getJSONObject(key))
            }
        } catch (e: JSONException) {
            logger.warn("Empty client info data.")
        }

        if (!includeClientId) {
            flattenedData.remove("client_id")
        }

        return flattenedData
    }

    /**
     * Collects the relevant data and assembles the requested ping.
     *
     * @param ping The metadata describing the ping to collect.
     * @return a string holding the data for the ping, or null if there is no data to send.
     */
    fun collect(ping: PingType): String? {
        logger.debug("Collecting ${ping.name}")
        val jsonPing = storageManager.collect(ping.name)

        // Return null if there is nothing in the jsonPing object so that this can be used by
        // consuming functions (i.e. sendPing()) to indicate no ping data is available to send.
        if (jsonPing.length() == 0) {
            return null
        }

        jsonPing.put("ping_info", getPingInfo(ping.name))
        jsonPing.put("client_info", getClientInfo(ping.includeClientId))

        return jsonPing.toString()
    }
}
