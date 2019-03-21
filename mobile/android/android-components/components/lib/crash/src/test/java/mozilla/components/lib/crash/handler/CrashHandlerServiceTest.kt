/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.handler

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import androidx.test.core.app.ApplicationProvider
import mozilla.components.lib.crash.Crash
import mozilla.components.lib.crash.CrashReporter
import mozilla.components.lib.crash.service.CrashReporterService
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.fail
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.doNothing
import org.mockito.Mockito.spy
import org.robolectric.RobolectricTestRunner

@RunWith(RobolectricTestRunner::class)
class CrashHandlerServiceTest {
    private val context: Context
        get() = ApplicationProvider.getApplicationContext()


    @After
    fun tearDown() {
        CrashReporter.reset()
    }

    @Test
    fun `CrashHandlerService will forward GeckoView crash to crash reporter`() {
        var caughtCrash: Crash.NativeCodeCrash? = null

        CrashReporter(
            shouldPrompt = CrashReporter.Prompt.NEVER,
            services = listOf(object : CrashReporterService {
                override fun report(crash: Crash.UncaughtExceptionCrash) {
                    fail("Didn't expect uncaught exception crash")
                }

                override fun report(crash: Crash.NativeCodeCrash) {
                    caughtCrash = crash
                }
            })
        ).install(context)

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

        val service = spy(CrashHandlerService())
        doNothing().`when`(service).kill()
        service.onHandleIntent(intent)

        assertNotNull(caughtCrash)

        val nativeCrash = caughtCrash
            ?: throw AssertionError("Expected NativeCodeCrash instance")

        assertEquals(true, nativeCrash.minidumpSuccess)
        assertEquals(false, nativeCrash.isFatal)
        assertEquals(
            "/data/data/org.mozilla.samples.browser/files/mozilla/Crash Reports/pending/3ba5f665-8422-dc8e-a88e-fc65c081d304.dmp",
            nativeCrash.minidumpPath
        )
        assertEquals(
            "/data/data/org.mozilla.samples.browser/files/mozilla/Crash Reports/pending/3ba5f665-8422-dc8e-a88e-fc65c081d304.extra",
            nativeCrash.extrasPath
        )
    }
}
