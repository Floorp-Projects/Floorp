/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.metrics.clientdeduplication

import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.ClientDeduplication
import org.mozilla.fenix.GleanMetrics.Pings
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

// For gleanTestRule
@RunWith(FenixRobolectricTestRunner::class)
internal class ClientDeduplicationPingTest {
    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    @Test
    fun `The clientDeduplication ping is sent`() {
        // Record test data.
        ClientDeduplication.validAdvertisingId.set(true)

        // Instruct the ping API to validate the ping data.
        var validatorRun = false
        Pings.clientDeduplication.testBeforeNextSubmit { reason ->
            assertEquals(Pings.clientDeduplicationReasonCodes.active, reason)
            assertEquals(true, ClientDeduplication.validAdvertisingId.testGetValue())
            validatorRun = true
        }
        Pings.clientDeduplication.submit(Pings.clientDeduplicationReasonCodes.active)

        // Verify that the validator ran.
        assertTrue(validatorRun)
    }
}
