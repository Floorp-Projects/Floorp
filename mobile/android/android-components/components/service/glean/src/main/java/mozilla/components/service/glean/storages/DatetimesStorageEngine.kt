/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.storages

import android.annotation.SuppressLint
import android.content.SharedPreferences
import mozilla.components.service.glean.private.CommonMetricData
import mozilla.components.service.glean.private.DatetimeMetricType
import mozilla.components.service.glean.utils.getISOTimeString
import mozilla.components.service.glean.utils.parseISOTimeString
import mozilla.components.support.base.log.logger.Logger
import java.util.Calendar
import java.util.Date

/**
 * This singleton handles the in-memory storage logic for datetimes. It is meant to be used by
 * the Specific Datetime API and the ping assembling objects.
 *
 * This stores dates both in-memory and on-disk as Strings, not Date objects. We do
 * this because we need to preserve the timezone offset value at the time the value
 * was set.  The [Date]/[Calendar] API in pre-Java 8 unfortunately does not allow
 * round-tripping the timezone offset when parsing a datetime string.  Since we don't
 * actually ever need to operate on the datetime's in a meaningful way, it's easiest
 * to just store the strings and treat them as opaque for everything but the testing API.
 */
@SuppressLint("StaticFieldLeak")
internal object DatetimesStorageEngine : DatetimesStorageEngineImplementation()

internal open class DatetimesStorageEngineImplementation(
    override val logger: Logger = Logger("glean/DatetimesStorageEngine")
) : GenericStorageEngine<String>() {

    override fun deserializeSingleMetric(metricName: String, value: Any?): String? {
        // This parses the date strings on ingestion as a sanity check, but we
        // don't actually need their results, and that would throw away the
        // timezone offset information.
        (value as? String)?.let {
            stringValue -> parseISOTimeString(stringValue)?.let {
                return stringValue
            }
        }
        return null
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
     * Set the metric to the provided date/time, truncating it to the
     * metric's resolution.
     *
     * @param metricData the metric information for the datetime
     * @param date the date value to set this metric to
     */
    fun set(metricData: DatetimeMetricType, date: Date = Date()) {
        super.recordMetric(
            metricData as CommonMetricData,
            getISOTimeString(date, metricData.timeUnit)
        )
    }

    /**
     * Set the metric to the provided date/time, truncating it to the
     * metric's resolution.
     *
     * @param metricData the metric information for the datetime
     * @param date the date value to set this metric to
     */
    fun set(metricData: DatetimeMetricType, calendar: Calendar) {
        super.recordMetric(
            metricData as CommonMetricData,
            getISOTimeString(calendar, metricData.timeUnit)
        )
    }
}
