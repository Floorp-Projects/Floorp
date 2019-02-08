/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.storages

import android.annotation.SuppressLint
import android.content.SharedPreferences
import mozilla.components.service.glean.CommonMetricData
import mozilla.components.service.glean.TimeUnit
import mozilla.components.service.glean.utils.getISOTimeString
import mozilla.components.service.glean.utils.parseISOTimeString

import mozilla.components.support.base.log.logger.Logger
import java.util.concurrent.TimeUnit as AndroidTimeUnit
import java.util.Date

/**
 * This singleton handles the in-memory storage logic for datetimes. It is meant to be used by
 * the Specific Datetime API and the ping assembling objects. No validation on the stored data
 * is performed at this point: validation must be performed by the Specific Datetime API.
 *
 * This class contains a reference to the Android application Context. While the IDE warns
 * us that this could leak, the application context lives as long as the application and this
 * object. For this reason, we should be safe to suppress the IDE warning.
 */
@SuppressLint("StaticFieldLeak")
internal object DatetimesStorageEngine : DatetimesStorageEngineImplementation()

internal open class DatetimesStorageEngineImplementation(
    override val logger: Logger = Logger("glean/DatetimesStorageEngine")
) : GenericScalarStorageEngine<Date>() {

    override fun deserializeSingleMetric(metricName: String, value: Any?): Date? {
        return if (value is String) parseISOTimeString(value) else null
    }

    override fun serializeSingleMetric(
        userPreferences: SharedPreferences.Editor?,
        storeName: String,
        value: Date,
        extraSerializationData: Any?
    ) {
        userPreferences?.putString(storeName, getISOTimeString(value))
    }

    /**
     * Set the metric to the provided date/time, truncating it to the
     * metric's resolution.
     *
     * @param date the date value to set this metric to
     * @param metricData the metric information for the datetime
     */
    fun set(date: Date, metricData: CommonMetricData) {
        // TODO: truncate the date given the precision in metricData.time_unit
        val truncatedDate = date
        super.recordScalar(metricData, truncatedDate)
    }
}
