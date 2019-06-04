/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.storages

import android.annotation.SuppressLint
import android.content.SharedPreferences
import mozilla.components.service.glean.error.ErrorRecording.ErrorType
import mozilla.components.service.glean.error.ErrorRecording.recordError
import mozilla.components.service.glean.private.CommonMetricData
import mozilla.components.support.base.log.logger.Logger

/**
 * This singleton handles the in-memory storage logic for strings. It is meant to be used by
 * the Specific Strings API and the ping assembling objects.
 *
 * This class contains a reference to the Android application Context. While the IDE warns
 * us that this could leak, the application context lives as long as the application and this
 * object. For this reason, we should be safe to suppress the IDE warning.
 */
@SuppressLint("StaticFieldLeak")
internal object StringsStorageEngine : StringsStorageEngineImplementation()

internal open class StringsStorageEngineImplementation(
    override val logger: Logger = Logger("glean/StringsStorageEngine")
) : GenericStorageEngine<String>() {
    companion object {
        // Maximum length of any passed value string, in characters.
        internal const val MAX_LENGTH_VALUE = 100
    }

    override fun deserializeSingleMetric(metricName: String, value: Any?): String? {
        return value as? String
    }

    override fun serializeSingleMetric(
        userPreferences: SharedPreferences.Editor?,
        storeName: String,
        value: String,
        extraSerializationData: Any?
    ) {
        userPreferences?.putString(storeName, value)
    }

    /**
     * Record a string in the desired stores.
     *
     * @param metricData object with metric settings
     * @param value the string value to record
     */
    fun record(
        metricData: CommonMetricData,
        value: String
    ) {
        val truncatedValue = value.let {
            if (it.length > MAX_LENGTH_VALUE) {
                recordError(
                    metricData,
                    ErrorType.InvalidValue,
                    "Value length ${it.length} exceeds maximum of $MAX_LENGTH_VALUE",
                    logger
                )
                return@let it.substring(0, MAX_LENGTH_VALUE)
            }
            it
        }

        super.recordMetric(metricData, truncatedValue)
    }
}
