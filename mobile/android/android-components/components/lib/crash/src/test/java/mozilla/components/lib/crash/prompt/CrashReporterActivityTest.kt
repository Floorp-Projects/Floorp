/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.prompt

import android.content.Context
import android.content.Intent
import android.widget.Button
import android.widget.CheckBox
import android.widget.TextView
import androidx.test.core.app.ApplicationProvider
import kotlinx.coroutines.delay
import kotlinx.coroutines.runBlocking
import mozilla.components.lib.crash.Crash
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.lib.crash.R
import mozilla.components.lib.crash.service.CrashReporterService
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify
import org.robolectric.Robolectric
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class CrashReporterActivityTest {
    private val context: Context
        get() = ApplicationProvider.getApplicationContext()

    @Test
    fun `Pressing close button sends report`() {
        val service: CrashReporterService = mock()

        CrashReporter(
            shouldPrompt = CrashReporter.Prompt.ALWAYS,
            services = listOf(service)
        ).install(context)

        val crash = Crash.UncaughtExceptionCrash(RuntimeException("Hello World"))

        val intent = Intent()
        crash.fillIn(intent)

        val activity = Robolectric.buildActivity(CrashReporterActivity::class.java, intent)
            .create()
            .visible()
            .start()
            .resume()
            .get()

        val closeButton = activity.findViewById<Button>(R.id.closeButton)
        closeButton.performClick()

        runBlocking {
            delay(100)
        }

        verify(service).report(crash)
    }

    @Test
    fun `Pressing restart button sends report`() {
        val service: CrashReporterService = mock()

        CrashReporter(
            shouldPrompt = CrashReporter.Prompt.ALWAYS,
            services = listOf(service)
        ).install(context)

        val crash = Crash.UncaughtExceptionCrash(RuntimeException("Hello World"))

        val intent = Intent()
        crash.fillIn(intent)

        val activity = Robolectric.buildActivity(CrashReporterActivity::class.java, intent)
            .create()
            .visible()
            .start()
            .resume()
            .get()

        val restartButton = activity.findViewById<Button>(R.id.restartButton)
        restartButton.performClick()

        runBlocking {
            delay(100)
        }

        verify(service).report(crash)
    }

    @Test
    fun `Custom message is set on CrashReporterActivity`() {
        CrashReporter(
            shouldPrompt = CrashReporter.Prompt.ALWAYS,
            promptConfiguration = CrashReporter.PromptConfiguration(
                message = "Hello World!",
                theme = android.R.style.Theme_DeviceDefault // Yolo!
            ),
            services = listOf(mock())
        ).install(context)

        val crash = Crash.UncaughtExceptionCrash(RuntimeException("Hello World"))

        val intent = Intent()
        crash.fillIn(intent)

        val activity = Robolectric.buildActivity(CrashReporterActivity::class.java, intent)
            .create()
            .visible()
            .start()
            .resume()
            .get()

        val view = activity.findViewById<TextView>(R.id.messageView)
        assertEquals("Hello World!", view.text)
    }

    @Test
    fun `Sending crash report saves checkbox state`() {
        val service: CrashReporterService = mock()

        CrashReporter(
            shouldPrompt = CrashReporter.Prompt.ALWAYS,
            services = listOf(service)
        ).install(context)

        val crash = Crash.UncaughtExceptionCrash(RuntimeException("Hello World"))

        val intent = Intent()
        crash.fillIn(intent)

        val activity = Robolectric.buildActivity(CrashReporterActivity::class.java, intent)
            .create()
            .visible()
            .start()
            .resume()
            .get()

        val checkBox = activity.findViewById<CheckBox>(R.id.sendCheckbox)
        checkBox.isChecked = true

        val preference = activity.getSharedPreferences("mozac_lib_crash_settings", Context.MODE_PRIVATE)
        assertFalse(preference.getBoolean("sendCrashReport", false))

        val restartButton = activity.findViewById<Button>(R.id.restartButton)
        restartButton.performClick()

        assertTrue(preference.getBoolean("sendCrashReport", false))
    }
}
