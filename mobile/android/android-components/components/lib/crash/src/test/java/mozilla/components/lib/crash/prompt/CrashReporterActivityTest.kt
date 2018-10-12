/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.prompt

import android.content.Intent
import android.widget.Button
import mozilla.components.lib.crash.Crash
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.lib.crash.R
import mozilla.components.lib.crash.service.CrashReporterService
import mozilla.components.support.test.mock
import org.junit.Ignore
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify
import org.robolectric.Robolectric
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment

@RunWith(RobolectricTestRunner::class)
@Ignore("Timing issues")
class CrashReporterActivityTest {
    @Test
    fun `Pressing close button sends report`() {
        val service: CrashReporterService = mock()

        CrashReporter(
            shouldPrompt = CrashReporter.Prompt.ALWAYS,
            services = listOf(service)
        ).install(RuntimeEnvironment.application)

        val crash = Crash.UncaughtExceptionCrash(RuntimeException("Hello World"))

        val intent = Intent()
        crash.fillIn(intent)

        val activity = Robolectric.buildActivity(CrashReporterActivity::class.java, intent)
            .create()
            .start()
            .resume()
            .get()

        val closeButton = activity.findViewById<Button>(R.id.closeButton)
        closeButton.performClick()

        verify(service).report(crash)
    }

    @Test
    fun `Pressing restart button sends report`() {
        val service: CrashReporterService = mock()

        CrashReporter(
            shouldPrompt = CrashReporter.Prompt.ALWAYS,
            services = listOf(service)
        ).install(RuntimeEnvironment.application)

        val crash = Crash.UncaughtExceptionCrash(RuntimeException("Hello World"))

        val intent = Intent()
        crash.fillIn(intent)

        val activity = Robolectric.buildActivity(CrashReporterActivity::class.java, intent)
            .create()
            .start()
            .resume()
            .get()

        val restartButton = activity.findViewById<Button>(R.id.restartButton)
        restartButton.performClick()

        verify(service).report(crash)
    }
}
