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
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertFalse
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.components.metrics.MetricController
import org.mozilla.fenix.helpers.FenixRobolectricTestRunner

@RunWith(FenixRobolectricTestRunner::class)
class DefaultBrowserIntentProcessorTest {

    @Test
    fun `do not process blank intents`() {
        val navController: NavController = mockk()
        val out: Intent = mockk()
        val result = DefaultBrowserIntentProcessor(mockk(), mockk())
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
        val metrics: MetricController = mockk()

        val intent = Intent().apply {
            putExtra("org.mozilla.fenix.default.browser.intent", true)
        }
        every { activity.startActivity(any()) } returns Unit
        every { activity.applicationContext } returns testContext
        every { metrics.track(any()) } returns Unit

        val result = DefaultBrowserIntentProcessor(activity, metrics)
            .process(intent, navController, out)

        assert(result)
        verify { metrics.track(any()) }
        verify { navController wasNot Called }
        verify { out wasNot Called }
    }
}
