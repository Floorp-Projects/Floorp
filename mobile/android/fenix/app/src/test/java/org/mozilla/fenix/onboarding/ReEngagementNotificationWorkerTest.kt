/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.onboarding

import io.mockk.spyk
import junit.framework.TestCase.assertFalse
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
        val localSetting = spyk(settings)

        localSetting.lastBrowseActivity = System.currentTimeMillis()
        assert(ReEngagementNotificationWorker.isActiveUser(localSetting))

        localSetting.lastBrowseActivity = System.currentTimeMillis() - Settings.FOUR_HOURS_MS
        assert(ReEngagementNotificationWorker.isActiveUser(localSetting))

        localSetting.lastBrowseActivity = System.currentTimeMillis() - Settings.ONE_DAY_MS
        assertFalse(ReEngagementNotificationWorker.isActiveUser(localSetting))

        localSetting.lastBrowseActivity = 0
        assertFalse(ReEngagementNotificationWorker.isActiveUser(localSetting))

        localSetting.lastBrowseActivity = -1000
        assertFalse(ReEngagementNotificationWorker.isActiveUser(localSetting))
    }
}
