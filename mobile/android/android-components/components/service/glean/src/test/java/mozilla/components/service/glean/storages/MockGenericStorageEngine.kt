/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.storages

import android.content.SharedPreferences
import mozilla.components.service.glean.private.CommonMetricData
import mozilla.components.support.base.log.logger.Logger

internal class MockGenericStorageEngine(
    override val logger: Logger = Logger("test")
) : GenericStorageEngine<Int>() {
    override fun deserializeSingleMetric(metricName: String, value: Any?): Int? {
        if (value is String) {
            return value.toIntOrNull()
        }

        return value as? Int?
    }

    override fun serializeSingleMetric(
        userPreferences: SharedPreferences.Editor?,
        storeName: String,
        value: Int,
        extraSerializationData: Any?
    ) {
        userPreferences?.putInt(storeName, value)
    }

    fun record(
        metricData: CommonMetricData,
        value: Int
    ) {
        super.recordMetric(metricData, value)
    }
}
