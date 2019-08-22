/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.intent

import android.content.Intent
import androidx.navigation.NavController
import io.mockk.Called
import io.mockk.mockk
import io.mockk.verify
import kotlinx.coroutines.ObsoleteCoroutinesApi
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.HomeActivity
import org.mozilla.fenix.NavGraphDirections
import org.mozilla.fenix.TestApplication
import org.mozilla.fenix.components.metrics.Event
import org.mozilla.fenix.components.metrics.MetricController
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@ObsoleteCoroutinesApi
@RunWith(RobolectricTestRunner::class)
@Config(application = TestApplication::class)
class StartSearchIntentProcessorTest {

    @Test
    fun `do not process blank intents`() {
        val metrics: MetricController = mockk()
        val navController: NavController = mockk()
        val out: Intent = mockk()
        StartSearchIntentProcessor(metrics).process(Intent(), navController, out)

        verify { metrics wasNot Called }
        verify { navController wasNot Called }
        verify { out wasNot Called }
    }

    @Test
    fun `do not process when search extra is false`() {
        val metrics: MetricController = mockk()
        val navController: NavController = mockk()
        val out: Intent = mockk()
        val intent = Intent().apply {
            putExtra(HomeActivity.OPEN_TO_SEARCH, false)
        }
        StartSearchIntentProcessor(metrics).process(intent, navController, out)

        verify { metrics wasNot Called }
        verify { navController wasNot Called }
        verify { out wasNot Called }
    }

    @Test
    fun `process search intents`() {
        val metrics: MetricController = mockk(relaxed = true)
        val navController: NavController = mockk(relaxed = true)
        val out: Intent = mockk(relaxed = true)
        val intent = Intent().apply {
            putExtra(HomeActivity.OPEN_TO_SEARCH, true)
        }
        StartSearchIntentProcessor(metrics).process(intent, navController, out)

        verify { metrics.track(Event.SearchWidgetNewTabPressed) }
        verify {
            navController.navigate(
                NavGraphDirections.actionGlobalSearch(sessionId = null, showShortcutEnginePicker = true)
            )
        }
        verify { out.putExtra(HomeActivity.OPEN_TO_SEARCH, false) }
    }
}
