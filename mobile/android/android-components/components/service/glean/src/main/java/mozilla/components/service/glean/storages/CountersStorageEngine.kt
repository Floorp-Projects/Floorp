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
 * This singleton handles the in-memory storage logic for counters. It is meant to be used by
 * the Specific Counters API and the ping assembling objects.
 *
 * This class contains a reference to the Android application Context. While the IDE warns
 * us that this could leak, the application context lives as long as the application and this
 * object. For this reason, we should be safe to suppress the IDE warning.
 */
@SuppressLint("StaticFieldLeak")
internal object CountersStorageEngine : CountersStorageEngineImplementation()

internal open class CountersStorageEngineImplementation(
    override val logger: Logger = Logger("glean/CountersStorageEngine")
) : GenericStorageEngine<Int>() {

    override fun deserializeSingleMetric(metricName: String, value: Any?): Int? {
        return (value as? Int)?.let {
            return@let if (it < 0) null else it
        }
    }

    override fun serializeSingleMetric(
        userPreferences: SharedPreferences.Editor?,
        storeName: String,
        value: Int,
        extraSerializationData: Any?
    ) {
        userPreferences?.putInt(storeName, value)
    }

    /**
     * Record a string in the desired stores.
     *
     * @param metricData object with metric settings
     * @param amount the integer amount to add to the currently stored value.  If there is
     * no current value, then the amount will be stored as the current value.
     */
    fun record(
        metricData: CommonMetricData,
        amount: Int
    ) {
        if (amount <= 0) {
            recordError(
                metricData,
                ErrorType.InvalidValue,
                "Added negative or zero value $amount",
                logger
            )
            return
        }

        // Use a custom combiner to add the amount to the existing counters rather than overwriting
        super.recordMetric(metricData, amount, null) { currentValue, newAmount ->
            currentValue?.let { it + newAmount } ?: newAmount
        }
    }
}
