/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.ping

import android.support.annotation.VisibleForTesting
import mozilla.components.service.glean.BuildConfig
import mozilla.components.service.glean.storages.StorageEngineManager
import mozilla.components.service.glean.storages.ExperimentsStorageEngine
import mozilla.components.support.base.log.logger.Logger
import mozilla.components.support.ktx.android.org.json.mergeWith
import org.json.JSONException
import org.json.JSONObject
import java.lang.StringBuilder
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

internal class PingMaker(
    private val storageManager: StorageEngineManager
) {
    private val logger = Logger("glean/PingMaker")
    private val pingStartTimes: MutableMap<String, String> = mutableMapOf()
    private val objectStartTime = getISOTimeString()

    /**
     * Generate an ISO8601 compliant time string for the current time.
     *
     * @return a string containing the date and time.
     */
    private fun getISOTimeString(): String {
        val dateFormat = SimpleDateFormat("yyyy-MM-dd'T'HH:mmZ", Locale.US)
        val timeString = StringBuilder(dateFormat.format(Date()))

        // Due to limitations of SDK version 21, there isn't a way to properly output the time
        // offset with a ':' character:
        // 2018-12-19T12:36-0600    --  This is what we get
        // 2018-12-19T12:36-06:00   -- This is what GCP will expect
        //
        // In order to satisfy time offset requirements of GCP, we manually insert the ":"
        timeString.insert(timeString.length - 2, ":")

        return timeString.toString()
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
        pingInfo.put("telemetry_sdk_build", BuildConfig.LIBRARY_VERSION)

        // Experiments belong in ping_info, because they must appear in every ping
        pingInfo.put("experiments", ExperimentsStorageEngine.getSnapshotAsJSON("", false))

        // TODO this section is still missing real app_build, seq and experiments.
        // These fields will be added by bug 1497894 when 1499756 and 1501318 land.
        pingInfo.put("seq", 0)
        pingInfo.put("app_build", "test-placeholder")

        pingInfo.mergeWith(getPingInfoMetrics())

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
     * Collect the metrics stored in the "glean_ping_info" bucket.
     *
     * @return a [JSONObject] containing the metrics belonging to the "ping_info"
     *         section of the ping.
     */
    private fun getPingInfoMetrics(): JSONObject {
        val pingInfoData = storageManager.collect("glean_ping_info")

        // The data returned by the manager is keyed by the storage engine name.
        // For example, the client id will live in the "uuid" object, within
        // `pingInfoData`. Remove the first level of indirection and return
        // the flattened data to the caller.
        val flattenedData = JSONObject()
        try {
            val metricsData = pingInfoData.getJSONObject("metrics")
            for (key in metricsData.keys()) {
                flattenedData.mergeWith(metricsData.getJSONObject(key))
            }
        } catch (e: JSONException) {
            logger.warn("Empty ping info data.")
        }

        return flattenedData
    }

    /**
     * Collects the relevant data and assembles the requested ping.
     *
     * @param storage the name of the storage containing the data for the ping.
     *        This usually matches with the name of the ping
     * @return a string holding the data for the ping.
     */
    fun collect(storage: String): String {
        val jsonPing = storageManager.collect(storage)
        jsonPing.put("ping_info", getPingInfo(storage))
        return jsonPing.toString()
    }
}
