/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.storages

import android.annotation.SuppressLint
import android.content.SharedPreferences
import mozilla.components.service.glean.error.ErrorRecording.recordError
import mozilla.components.service.glean.error.ErrorRecording.ErrorType
import mozilla.components.service.glean.private.CommonMetricData
import mozilla.components.support.base.log.logger.Logger

/**
 * This singleton handles the in-memory storage logic for quantities. It is meant to be used by
 * the Specific Quantities API and the ping assembling objects.
 *
 * This class contains a reference to the Android application Context. While the IDE warns
 * us that this could leak, the application context lives as long as the application and this
 * object. For this reason, we should be safe to suppress the IDE warning.
 */
@SuppressLint("StaticFieldLeak")
internal object QuantitiesStorageEngine : QuantitiesStorageEngineImplementation()

internal open class QuantitiesStorageEngineImplementation(
    override val logger: Logger = Logger("glean/QuantitiesStorageEngine")
) : GenericStorageEngine<Long>() {

    override fun deserializeSingleMetric(metricName: String, value: Any?): Long? {
        return (value as? Long)?.let {
            return@let if (it < 0) null else it
        }
    }

    override fun serializeSingleMetric(
        userPreferences: SharedPreferences.Editor?,
        storeName: String,
        value: Long,
        extraSerializationData: Any?
    ) {
        userPreferences?.putLong(storeName, value)
    }

    /**
     * Record a string in the desired stores.
     *
     * @param metricData object with metric settings
     * @param value the value to set. Must be non-negative.
     */
    fun record(
        metricData: CommonMetricData,
        value: Long
    ) {
        if (value < 0) {
            recordError(
                metricData,
                ErrorType.InvalidValue,
                "Set negative value $value",
                logger
            )
            return
        }

        super.recordMetric(metricData, value)
    }
}
