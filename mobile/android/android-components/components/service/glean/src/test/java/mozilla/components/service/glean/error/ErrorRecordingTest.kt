/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.service.glean.error

import androidx.test.core.app.ApplicationProvider
import mozilla.components.service.glean.private.Lifetime
import mozilla.components.service.glean.private.StringMetricType
import mozilla.components.service.glean.storages.CountersStorageEngine
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.support.base.log.logger.Logger
import org.junit.Assert.assertEquals
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class ErrorRecordingTest {

    @get:Rule
    val gleanRule = GleanTestRule(ApplicationProvider.getApplicationContext())

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

        val expectedErrors = mapOf(
            ErrorRecording.ErrorType.InvalidValue to 1,
            ErrorRecording.ErrorType.InvalidLabel to 2
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
            logger,
            numErrors = expectedErrors[ErrorRecording.ErrorType.InvalidLabel]
        )

        for (storeName in listOf("store1", "store2", "metrics")) {
            for (errorType in expectedErrors.keys) {
                assertEquals(
                    expectedErrors[errorType],
                    ErrorRecording.testGetNumRecordedErrors(stringMetric, errorType, storeName)
                )
            }
        }
    }
}
