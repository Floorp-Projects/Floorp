/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics

import android.content.Context.MODE_PRIVATE
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.ext.application
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class LeanplumMetricsServiceTest {
    @Test
    fun `deviceId is only generated on first run`() {
        var callCount = 0
        val idGenerator = {
            callCount++
            "TEST_DEVICE_ID"
        }

        val sharedPreferences = testContext.application.getSharedPreferences(
            "LEANPLUM_PREFERENCES",
            MODE_PRIVATE
        )

        assertNull(sharedPreferences.getString("LP_DEVICE_ID", null))

        val leanplumMetricService = LeanplumMetricsService(testContext.application, idGenerator)
        assertEquals("TEST_DEVICE_ID", leanplumMetricService.deviceId)

        val leanplumMetricService2 = LeanplumMetricsService(testContext.application, idGenerator)
        assertEquals("TEST_DEVICE_ID", leanplumMetricService2.deviceId)
        assertEquals(1, callCount)

        assertEquals("TEST_DEVICE_ID", sharedPreferences.getString("LP_DEVICE_ID", ""))
    }
}
