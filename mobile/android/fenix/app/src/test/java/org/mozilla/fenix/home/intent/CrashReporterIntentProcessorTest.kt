/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.intent

import android.content.Intent
import android.os.Bundle
import androidx.navigation.NavController
import io.mockk.Called
import io.mockk.mockk
import io.mockk.verify
import kotlinx.coroutines.ObsoleteCoroutinesApi
import org.junit.Test
import org.junit.runner.RunWith
import org.mozilla.fenix.NavGraphDirections
import org.mozilla.fenix.TestApplication
import org.robolectric.RobolectricTestRunner
import org.robolectric.annotation.Config

@ObsoleteCoroutinesApi
@RunWith(RobolectricTestRunner::class)
@Config(application = TestApplication::class)
class CrashReporterIntentProcessorTest {

    @Test
    fun `do not process blank intents`() {
        val navController: NavController = mockk()
        val out: Intent = mockk()
        CrashReporterIntentProcessor().process(Intent(), navController, out)

        verify { navController wasNot Called }
        verify { out wasNot Called }
    }

    @Test
    fun `process crash intents`() {
        val navController: NavController = mockk(relaxed = true)
        val out: Intent = mockk()
        val intent = Intent().apply {
            putExtra("mozilla.components.lib.crash.CRASH", mockk<Bundle>())
        }
        CrashReporterIntentProcessor().process(intent, navController, out)

        verify { navController.navigate(NavGraphDirections.actionGlobalCrashReporter(intent)) }
        verify { out wasNot Called }
    }
}
