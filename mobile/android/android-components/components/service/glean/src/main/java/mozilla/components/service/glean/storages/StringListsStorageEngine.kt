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
import mozilla.components.support.ktx.android.org.json.toList
import org.json.JSONArray

/**
 * This singleton handles the in-memory storage logic for string lists. It is meant to be used by
 * the Specific String List API and the ping assembling objects.
 *
 * This class contains a reference to the Android application Context. While the IDE warns
 * us that this could leak, the application context lives as long as the application and this
 * object. For this reason, we should be safe to suppress the IDE warning.
 */
@SuppressLint("StaticFieldLeak")
internal object StringListsStorageEngine : StringListsStorageEngineImplementation()

internal open class StringListsStorageEngineImplementation(
    override val logger: Logger = Logger("glean/StringsListsStorageEngine")
) : GenericStorageEngine<List<String>>() {
    companion object {
        // Maximum length of any list
        const val MAX_LIST_LENGTH_VALUE = 20
        // Maximum length of any string in the list
        const val MAX_STRING_LENGTH = 50
    }

    override fun deserializeSingleMetric(metricName: String, value: Any?): List<String>? {
        /*
        Since SharedPreferences doesn't directly support storing of List<> types, we must use
        an intermediate JSONArray which can be deserialized and converted back to List<String>.
        Using JSONArray introduces a possible issue in that it's constructor will still properly
        convert a stringified JSONArray into an array of Strings regardless of whether the values
        have been properly quoted or not.  For example, [1,2,3] is as valid just like
        ["a","b","c"] is valid.

        The try/catch is necessary as JSONArray can throw a JSONException if it cannot parse the
        string into an array.
        */
        return (value as? String)?.let {
            try {
                return@let JSONArray(it).toList()
            } catch (e: org.json.JSONException) {
                return@let null
            }
        }
    }

    override fun serializeSingleMetric(
        userPreferences: SharedPreferences.Editor?,
        storeName: String,
        value: List<String>,
        extraSerializationData: Any?
    ) {
        // Since SharedPreferences doesn't directly support storing of List<> types, we must use
        // an intermediate JSONArray which can be serialized to a String type and stored.
        val jsonArray = JSONArray(value)
        userPreferences?.putString(storeName, jsonArray.toString())
    }

    /**
     * Appends to an existing string list in the desired stores.  If the store or list doesn't exist
     * then it is created and added to the desired stores.
     *
     * @param metricData object with metric settings
     * @param value the string list value to add
     */
    fun add(
        metricData: CommonMetricData,
        value: String
    ) {
        val truncatedValue = value.let {
            if (it.length > MAX_STRING_LENGTH) {
                recordError(
                    metricData,
                    ErrorType.InvalidValue,
                    "Individual value length ${it.length} exceeds maximum of $MAX_STRING_LENGTH",
                    logger
                )
                return@let it.substring(0, MAX_STRING_LENGTH)
            }
            it
        }

        // Use a custom combiner to add the string to the existing list rather than overwriting
        super.recordMetric(metricData, listOf(truncatedValue), null) { currentValue, newValue ->
            currentValue?.let {
                if (it.count() + 1 > MAX_LIST_LENGTH_VALUE) {
                    recordError(
                        metricData,
                        ErrorType.InvalidValue,
                        "String list length of ${it.count() + 1} exceeds maximum of $MAX_LIST_LENGTH_VALUE",
                        logger
                    )
                }

                it + newValue.take(MAX_LIST_LENGTH_VALUE - it.count())
            } ?: newValue
        }
    }

    /**
     * Sets a string list in the desired stores. This function will replace the existing list or
     * create a new list if it doesn't already exist. To add or append to an existing list, use
     * [add] function. If an empty list is passed in, then an [ErrorType.InvalidValue] will be
     * generated and the method will return without recording.
     *
     * @param metricData object with metric settings
     * @param value the string list value to record
     */
    fun set(
        metricData: CommonMetricData,
        value: List<String>
    ) {
        val stringList = value.map {
            if (it.length > MAX_STRING_LENGTH) {
                recordError(
                    metricData,
                    ErrorType.InvalidValue,
                    "String too long ${it.length} > $MAX_STRING_LENGTH",
                    logger
                )
            }
            it.take(MAX_STRING_LENGTH)
        }

        // Record an error when attempting to record a zero-length list and return.
        if (stringList.count() == 0) {
            recordError(
                metricData,
                ErrorType.InvalidValue,
                "Attempt to set() an empty string list to ${metricData.identifier}",
                logger
            )
            return
        }

        if (stringList.count() > MAX_LIST_LENGTH_VALUE) {
            recordError(
                metricData,
                ErrorType.InvalidValue,
                "String list length of ${value.count()} exceeds maximum of $MAX_LIST_LENGTH_VALUE",
                logger
            )
        }

        super.recordMetric(metricData, stringList.take(MAX_LIST_LENGTH_VALUE))
    }
}
