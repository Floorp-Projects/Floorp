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
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.GleanMetrics.Events
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

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

        assertFalse(Events.defaultBrowserNotifTapped.testHasValue())

        val result = DefaultBrowserIntentProcessor(activity)
            .process(intent, navController, out)

        assert(result)

        assertTrue(Events.defaultBrowserNotifTapped.testHasValue())
        verify { navController wasNot Called }
        verify { out wasNot Called }
    }
}
