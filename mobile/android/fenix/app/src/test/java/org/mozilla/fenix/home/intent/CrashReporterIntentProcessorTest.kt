/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.intent

import android.content.Intent
import androidx.navigation.NavController
import io.mockk.Called
import io.mockk.every
import io.mockk.mockk
import io.mockk.mockkObject
import io.mockk.verify
import mozilla.components.lib.crash.Crash
import mozilla.components.lib.crash.Crash.NativeCodeCrash
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.mozilla.fenix.components.AppStore
import org.mozilla.fenix.components.appstate.AppAction

class CrashReporterIntentProcessorTest {
    private val appStore: AppStore = mockk(relaxed = true)
    private val navController: NavController = mockk()
    private val out: Intent = mockk()

    @Test
    fun `GIVEN a blank Intent WHEN processing it THEN do nothing and return false`() {
        val processor = CrashReporterIntentProcessor(appStore)

        val result = processor.process(Intent(), navController, out)

        assertFalse(result)
        verify { navController wasNot Called }
        verify { out wasNot Called }
        verify { appStore wasNot Called }
    }

    @Test
    fun `GIVEN a crash Intent WHEN processing it THEN update crash details and return true`() {
        val processor = CrashReporterIntentProcessor(appStore)
        val intent = Intent()
        val crash = mockk<NativeCodeCrash>(relaxed = true)

        mockkObject(Crash.Companion) {
            every { Crash.Companion.isCrashIntent(intent) } returns true
            every { Crash.Companion.fromIntent(intent) } returns crash

            val result = processor.process(intent, navController, out)

            assertTrue(result)
            verify { navController wasNot Called }
            verify { out wasNot Called }
            verify { appStore.dispatch(AppAction.AddNonFatalCrash(crash)) }
        }
    }
}
