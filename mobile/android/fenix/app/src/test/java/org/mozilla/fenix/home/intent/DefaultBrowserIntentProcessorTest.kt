/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.intent

import android.content.Intent
import androidx.navigation.NavController
import io.mockk.Called
import io.mockk.every
import io.mockk.mockk
import io.mockk.verify
import mozilla.components.concept.engine.EngineSession
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.BrowserDirection
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.browser.browsingmode.BrowsingModeManager
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner
import org.mozilla.fenix.onboarding.ReEngagementNotificationWorker

@RunWith(FenixRobolectricTestRunner::class)
class DefaultBrowserIntentProcessorTest {

    @get:Rule
    val gleanTestRule = GleanTestRule(testContext)

    @Test
    fun `do not process blank intents`() {
        val navController: NavController = mockk()
        val out: Intent = mockk()
        val result = DefaultBrowserIntentProcessor(mockk())
            .process(Intent(), navController, out)

        assertFalse(result)
        verify { navController wasNot Called }
        verify { out wasNot Called }
    }

    @Test
    fun `process default browser notification intents`() {
        val navController: NavController = mockk(relaxed = true)
        val out: Intent = mockk()
        val activity: HomeActivity = mockk()

        val intent = Intent().apply {
            putExtra("org.mozilla.fenix.default.browser.intent", true)
        }
        every { activity.startActivity(any()) } returns Unit
        every { activity.applicationContext } returns testContext

        assertNull(Events.defaultBrowserNotifTapped.testGetValue())

        val result = DefaultBrowserIntentProcessor(activity)
            .process(intent, navController, out)

        assert(result)

        assertNotNull(Events.defaultBrowserNotifTapped.testGetValue())
        verify { navController wasNot Called }
        verify { out wasNot Called }
    }

    @Test
    fun `process re-engagement notification intents`() {
        val navController: NavController = mockk(relaxed = true)
        val out: Intent = mockk()
        val activity: HomeActivity = mockk(relaxed = true)
        val browsingModeManager: BrowsingModeManager = mockk(relaxed = true)

        val intent = Intent().apply {
            putExtra("org.mozilla.fenix.re-engagement.intent", true)
        }
        every { activity.applicationContext } returns testContext
        every { activity.browsingModeManager } returns browsingModeManager

        assertNull(Events.reEngagementNotifTapped.testGetValue())

        val result = DefaultBrowserIntentProcessor(activity)
            .process(intent, navController, out)

        assert(result)

        assertNotNull(Events.reEngagementNotifTapped.testGetValue())
        verify {
            activity.openToBrowserAndLoad(
                searchTermOrURL = ReEngagementNotificationWorker.NOTIFICATION_TARGET_URL,
                newTab = true,
                from = BrowserDirection.FromGlobal,
                customTabSessionId = null,
                engine = null,
                forceSearch = false,
                flags = EngineSession.LoadUrlFlags.external(),
                requestDesktopMode = false,
                historyMetadata = null,
            )
        }
        verify { navController wasNot Called }
        verify { out wasNot Called }
    }
}
