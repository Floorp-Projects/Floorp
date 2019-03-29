/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.error

import mozilla.components.service.glean.Glean
import mozilla.components.service.glean.metrics.Lifetime
import mozilla.components.service.glean.metrics.StringMetricType
import mozilla.components.service.glean.resetGlean
import mozilla.components.service.glean.storages.CountersStorageEngine
import mozilla.components.support.base.log.logger.Logger
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class ErrorRecordingTest {
    @Before
    fun setup() {
        resetGlean()
    }

    @After
    fun resetGlobalState() {
        Glean.setUploadEnabled(true)
    }

    @Test
    fun `test recording of all error types`() {
        CountersStorageEngine.clearAllStores()
        val logger = Logger("glean/ErrorRecordingTest")

        val stringMetric = StringMetricType(
            disabled = false,
            category = "telemetry",
            lifetime = Lifetime.Application,
            name = "string_metric",
            sendInPings = listOf("store1", "store2")
        )

        ErrorRecording.recordError(
            stringMetric,
            ErrorRecording.ErrorType.InvalidValue,
            "Invalid value",
            logger
        )

        ErrorRecording.recordError(
            stringMetric,
            ErrorRecording.ErrorType.InvalidLabel,
            "Invalid label",
            logger
        )

        for (storeName in listOf("store1", "store2", "metrics")) {
            for (errorType in listOf(
                ErrorRecording.ErrorType.InvalidValue,
                ErrorRecording.ErrorType.InvalidLabel
            )) {
                assertEquals(
                    1,
                    ErrorRecording.testGetNumRecordedErrors(stringMetric, errorType, storeName)
                )
            }
        }
    }
}
