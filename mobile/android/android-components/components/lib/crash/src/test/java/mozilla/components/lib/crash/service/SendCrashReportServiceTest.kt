/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.service

import android.content.ComponentName
import android.content.Intent
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.lib.crash.Crash
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.robolectric.testContext
import org.junit.After
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.robolectric.Robolectric

@RunWith(AndroidJUnit4::class)
class SendCrashReportServiceTest {
    private var service: SendCrashReportService? = null

    @Before
    fun setUp() {
        service = spy(Robolectric.setupService(SendCrashReportService::class.java))
        service?.startService(Intent())
    }

    @After
    fun tearDown() {
        service?.stopService(Intent())
        CrashReporter.reset()
    }

    @Test
    fun `CrashRHandlerService will forward same crash to crash reporter`() {
        spy(CrashReporter(
                shouldPrompt = CrashReporter.Prompt.ALWAYS,
                services = listOf(object : CrashReporterService {
                    override fun report(crash: Crash.UncaughtExceptionCrash) {
                    }

                    override fun report(crash: Crash.NativeCodeCrash) {
                    }
                }))
        ).install(testContext)
        val originalCrash = Crash.NativeCodeCrash(
                "/data/data/org.mozilla.samples.browser/files/mozilla/Crash Reports/pending/3ba5f665-8422-dc8e-a88e-fc65c081d304.dmp",
                true,
                "/data/data/org.mozilla.samples.browser/files/mozilla/Crash Reports/pending/3ba5f665-8422-dc8e-a88e-fc65c081d304.extra",
                false
        )

        val intent = Intent("org.mozilla.gecko.ACTION_CRASHED")
        intent.component = ComponentName(
                "org.mozilla.samples.browser",
                "mozilla.components.lib.crash.handler.CrashHandlerService"
        )
        intent.putExtra(
                "minidumpPath",
                "/data/data/org.mozilla.samples.browser/files/mozilla/Crash Reports/pending/3ba5f665-8422-dc8e-a88e-fc65c081d304.dmp"
        )
        intent.putExtra("fatal", false)
        intent.putExtra(
                "extrasPath",
                "/data/data/org.mozilla.samples.browser/files/mozilla/Crash Reports/pending/3ba5f665-8422-dc8e-a88e-fc65c081d304.extra"
        )
        intent.putExtra("minidumpSuccess", true)
        originalCrash.fillIn(intent)

        service?.onStartCommand(intent, 0, 0)
        verify(service)?.sendCrashReport(eq(originalCrash), any())
    }
}
