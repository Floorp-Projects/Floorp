/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.storages

import android.annotation.SuppressLint
import android.content.SharedPreferences
import mozilla.components.service.glean.private.CommonMetricData
import mozilla.components.support.base.log.logger.Logger

/**
 * This singleton handles the in-memory storage logic for booleans. It is meant to be used by
 * the Specific Booleans API and the ping assembling objects.
 *
 * This class contains a reference to the Android application Context. While the IDE warns
 * us that this could leak, the application context lives as long as the application and this
 * object. For this reason, we should be safe to suppress the IDE warning.
 */
@SuppressLint("StaticFieldLeak")
internal object BooleansStorageEngine : BooleansStorageEngineImplementation()

internal open class BooleansStorageEngineImplementation(
    override val logger: Logger = Logger("glean/BooleansStorageEngine")
) : GenericStorageEngine<Boolean>() {

    override fun deserializeSingleMetric(metricName: String, value: Any?): Boolean? {
        return value as? Boolean
    }

    override fun serializeSingleMetric(
        userPreferences: SharedPreferences.Editor?,
        storeName: String,
        value: Boolean,
        extraSerializationData: Any?
    ) {
        userPreferences?.putBoolean(storeName, value)
    }

    /**
     * Record a boolean in the desired stores.
     *
     * @param metricData object with metric settings
     * @param value the boolean value to record
     */
    fun record(
        metricData: CommonMetricData,
        value: Boolean
    ) {
        super.recordMetric(metricData, value)
    }
}
