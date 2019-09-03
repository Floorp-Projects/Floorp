/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash

import android.app.Activity
import android.app.PendingIntent
import android.content.Intent
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.lib.crash.service.CrashReporterService
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.expectException
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import org.robolectric.Robolectric
import org.robolectric.Shadows.shadowOf
import java.lang.reflect.Modifier

@RunWith(AndroidJUnit4::class)
class CrashReporterTest {

    @Before
    fun setUp() {
        CrashReporter.reset()
    }

    @Test
    fun `Calling install() will setup uncaught exception handler`() {
        val defaultHandler = Thread.getDefaultUncaughtExceptionHandler()

        CrashReporter(
            services = listOf(mock())
        ).install(testContext)

        val newHandler = Thread.getDefaultUncaughtExceptionHandler()
        assertNotNull(newHandler)

        assertNotEquals(defaultHandler, newHandler)
    }

    @Test(expected = IllegalArgumentException::class)
    fun `CrashReporter throws if no service is defined`() {
        CrashReporter(emptyList())
            .install(testContext)
    }

    @Test
    fun `CrashReporter will submit report immediately if setup with Prompt-NEVER`() {
        val service: CrashReporterService = mock()

        val reporter = spy(CrashReporter(
            services = listOf(service),
            shouldPrompt = CrashReporter.Prompt.NEVER
        ).install(testContext))

        val crash: Crash.UncaughtExceptionCrash = mock()

        reporter.onCrash(testContext, crash)

        verify(reporter).submitReport(crash)
        verify(reporter, never()).showPrompt(any(), eq(crash))

        verify(service).report(crash)
    }

    @Test
    fun `CrashReporter will show prompt if setup with Prompt-ALWAYS`() {
        val service: CrashReporterService = mock()

        val reporter = spy(CrashReporter(
            services = listOf(service),
            shouldPrompt = CrashReporter.Prompt.ALWAYS
        ).install(testContext))

        val crash: Crash.UncaughtExceptionCrash = mock()

        reporter.onCrash(testContext, crash)

        verify(reporter, never()).submitReport(crash)
        verify(reporter).showPrompt(any(), eq(crash))

        verify(service, never()).report(crash)
    }

    @Test
    fun `CrashReporter will submit report immediately for non native crash and with setup Prompt-ONLY_NATIVE_CRASH`() {
        val service: CrashReporterService = mock()

        val reporter = spy(CrashReporter(
            services = listOf(service),
            shouldPrompt = CrashReporter.Prompt.ONLY_NATIVE_CRASH
        ).install(testContext))

        val crash: Crash.UncaughtExceptionCrash = mock()

        reporter.onCrash(testContext, crash)

        verify(reporter).submitReport(crash)
        verify(reporter, never()).showPrompt(any(), eq(crash))

        verify(service).report(crash)
    }

    @Test
    fun `CrashReporter will show prompt for native crash and with setup Prompt-ONLY_NATIVE_CRASH`() {
        val service: CrashReporterService = mock()

        val reporter = spy(CrashReporter(
            services = listOf(service),
            shouldPrompt = CrashReporter.Prompt.ONLY_NATIVE_CRASH
        ).install(testContext))

        val crash: Crash.NativeCodeCrash = mock()

        reporter.onCrash(testContext, crash)

        verify(reporter, never()).submitReport(crash)
        verify(reporter).showPrompt(any(), eq(crash))

        verify(service, never()).report(crash)
    }

    @Test
    fun `CrashReporter is enabled by default`() {
        val reporter = spy(CrashReporter(
            services = listOf(mock()),
            shouldPrompt = CrashReporter.Prompt.ONLY_NATIVE_CRASH
        ).install(testContext))

        assertTrue(reporter.enabled)
    }

    @Test
    fun `CrashReporter will not prompt and not submit report if not enabled`() {
        val service: CrashReporterService = mock()

        val reporter = spy(CrashReporter(
            services = listOf(service),
            shouldPrompt = CrashReporter.Prompt.ALWAYS
        ).install(testContext))

        reporter.enabled = false

        val crash: Crash.UncaughtExceptionCrash = mock()
        reporter.onCrash(testContext, crash)

        verify(reporter, never()).submitReport(crash)
        verify(reporter, never()).showPrompt(any(), eq(crash))

        verify(service, never()).report(crash)
    }

    @Test
    fun `CrashReporter forwards crashes to service`() {
        var nativeCrash = false
        var exceptionCrash = false

        val service = object : CrashReporterService {
            override fun report(crash: Crash.UncaughtExceptionCrash) {
                exceptionCrash = true
                nativeCrash = false
            }

            override fun report(crash: Crash.NativeCodeCrash) {
                exceptionCrash = false
                nativeCrash = true
            }
        }

        val reporter = spy(CrashReporter(
            services = listOf(service),
            shouldPrompt = CrashReporter.Prompt.NEVER
        ).install(testContext))

        reporter.onCrash(
            mock(),
            Crash.UncaughtExceptionCrash(RuntimeException(), arrayListOf()))

        assertTrue(exceptionCrash)
        assertFalse(nativeCrash)

        reporter.onCrash(
            mock(),
            Crash.NativeCodeCrash("", true, "", false, arrayListOf())
        )

        assertFalse(exceptionCrash)
        assertTrue(nativeCrash)
    }

    @Test
    fun `Internal reference is set after calling install`() {
        expectException(IllegalStateException::class) {
            CrashReporter.requireInstance
        }

        val reporter = CrashReporter(
            services = listOf(mock())
        )

        expectException(IllegalStateException::class) {
            CrashReporter.requireInstance
        }

        reporter.install(testContext)

        assertNotNull(CrashReporter.requireInstance)
    }

    @Test
    fun `CrashReporter invokes PendingIntent if provided`() {
        val context = Robolectric.setupActivity(Activity::class.java)

        val intent = Intent("action")
        val pendingIntent = spy(PendingIntent.getActivity(context, 0, intent, 0))

        val reporter = CrashReporter(
            shouldPrompt = CrashReporter.Prompt.ALWAYS,
            services = listOf(mock()),
            nonFatalCrashIntent = pendingIntent
        ).install(context)

        val nativeCrash = Crash.NativeCodeCrash(
            "dump.path",
            true,
            "extras.path",
            isFatal = false,
            breadcrumbs = arrayListOf())
        reporter.onCrash(context, nativeCrash)

        verify(pendingIntent).send(eq(context), eq(0), any())

        val receivedIntent = shadowOf(context).nextStartedActivity

        val receivedCrash = Crash.fromIntent(receivedIntent) as? Crash.NativeCodeCrash
            ?: throw AssertionError("Expected NativeCodeCrash instance")

        assertEquals(nativeCrash, receivedCrash)
        assertEquals("dump.path", receivedCrash.minidumpPath)
        assertEquals(true, receivedCrash.minidumpSuccess)
        assertEquals("extras.path", receivedCrash.extrasPath)
        assertEquals(false, receivedCrash.isFatal)
    }

    @Test
    fun `CrashReporter instance writes are visible across threads`() {
        val instanceField = CrashReporter::class.java.getDeclaredField("instance")
        assertTrue(Modifier.isVolatile(instanceField.modifiers))
    }
}
