/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.session

import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class SessionNotificationServiceTest {

    @Test
    fun `Service keeps tracked of started state`() {
        assertFalse(SessionNotificationService.started)

        SessionNotificationService.start(testContext, false)
        assertTrue(SessionNotificationService.started)

        SessionNotificationService.stop(testContext)
        assertFalse(SessionNotificationService.started)
    }
}
