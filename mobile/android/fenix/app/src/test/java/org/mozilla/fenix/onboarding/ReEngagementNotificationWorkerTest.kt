/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding

import junit.framework.TestCase.assertFalse
import junit.framework.TestCase.assertTrue
import mozilla.components.support.test.robolectric.testContext
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.utils.Settings

@RunWith(FenixRobolectricTestRunner::class)
class ReEngagementNotificationWorkerTest {
    lateinit var settings: Settings

    @Before
    fun setUp() {
        settings = Settings(testContext)
    }

    @Test
    fun `GIVEN last browser activity THEN determine if the user is active correctly`() {
        val now = System.currentTimeMillis()
        val fourHoursAgo = now - Settings.FOUR_HOURS_MS
        val oneDayAgo = now - Settings.ONE_DAY_MS

        assertTrue(ReEngagementNotificationWorker.isActiveUser(now, now))
        assertTrue(ReEngagementNotificationWorker.isActiveUser(fourHoursAgo, now))

        // test inactive user threshold
        assertTrue(ReEngagementNotificationWorker.isActiveUser(oneDayAgo, now))
        assertFalse(ReEngagementNotificationWorker.isActiveUser(oneDayAgo - 1, now))
        assertTrue(ReEngagementNotificationWorker.isActiveUser(oneDayAgo + 1, now))

        // test default value
        assertFalse(ReEngagementNotificationWorker.isActiveUser(0, now))

        // test unlikely value
        assertFalse(ReEngagementNotificationWorker.isActiveUser(-1000, now))
    }
}
