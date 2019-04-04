/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.storages

import android.annotation.SuppressLint
import android.content.SharedPreferences
import mozilla.components.service.glean.private.CommonMetricData
import mozilla.components.support.base.log.logger.Logger
import java.util.UUID

/**
 * This singleton handles the in-memory storage logic for uuids. It is meant to be used by
 * the Specific UUID API and the ping assembling objects.
 *
 * This class contains a reference to the Android application Context. While the IDE warns
 * us that this could leak, the application context lives as long as the application and this
 * object. For this reason, we should be safe to suppress the IDE warning.
 */
@SuppressLint("StaticFieldLeak")
internal object UuidsStorageEngine : UuidsStorageEngineImplementation()

internal open class UuidsStorageEngineImplementation(
    override val logger: Logger = Logger("glean/UuidsStorageEngine")
) : GenericStorageEngine<UUID>() {

    override fun deserializeSingleMetric(metricName: String, value: Any?): UUID? {
        return try {
            if (value is String) UUID.fromString(value) else null
        } catch (e: IllegalArgumentException) {
            null
        }
    }

    override fun serializeSingleMetric(
        userPreferences: SharedPreferences.Editor?,
        storeName: String,
        value: UUID,
        extraSerializationData: Any?
    ) {
        userPreferences?.putString(storeName, value.toString())
    }

    /**
     * Record a uuid in the desired stores.
     *
     * @param metricData object with metric settings
     * @param value the uuid value to record
     */
    fun record(
        metricData: CommonMetricData,
        value: UUID
    ) {
        super.recordMetric(metricData, value)
    }
}
