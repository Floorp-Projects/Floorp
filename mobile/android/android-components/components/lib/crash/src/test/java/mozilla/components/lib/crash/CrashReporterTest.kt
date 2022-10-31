/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash

import android.app.Activity
import android.app.PendingIntent
import android.content.Intent
import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.ExperimentalCoroutinesApi
import mozilla.components.concept.base.crash.Breadcrumb
import mozilla.components.lib.crash.service.CrashReporterService
import mozilla.components.lib.crash.service.CrashTelemetryService
import mozilla.components.support.test.any
import mozilla.components.support.test.eq
import mozilla.components.support.test.expectException
import mozilla.components.support.test.ext.joinBlocking
import mozilla.components.support.test.mock
import mozilla.components.support.test.robolectric.testContext
import mozilla.components.support.test.rule.MainCoroutineRule
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.never
import org.mockito.Mockito.spy
import org.mockito.Mockito.times
import org.mockito.Mockito.verify
import org.robolectric.Robolectric
import org.robolectric.Shadows.shadowOf
import java.lang.Thread.sleep
import java.lang.reflect.Modifier

@ExperimentalCoroutinesApi
@RunWith(AndroidJUnit4::class)
class CrashReporterTest {

    @get:Rule
    val coroutinesTestRule = MainCoroutineRule()
    private val scope = coroutinesTestRule.scope

    @Before
    fun setUp() {
        CrashReporter.reset()
    }

    @Test
    fun `Calling install() will setup uncaught exception handler`() {
        val defaultHandler = Thread.getDefaultUncaughtExceptionHandler()

        CrashReporter(
            context = testContext,
            services = listOf(mock()),
        ).install(testContext)

        val newHandler = Thread.getDefaultUncaughtExceptionHandler()
        assertNotNull(newHandler)

        assertNotEquals(defaultHandler, newHandler)
    }

    @Test(expected = IllegalArgumentException::class)
    fun `CrashReporter throws if no service is defined`() {
        CrashReporter(
            context = testContext,
            services = emptyList(),
        ).install(testContext)
    }

    @Test
    fun `CrashReporter will submit report immediately if setup with Prompt-NEVER`() {
        val service: CrashReporterService = mock()
        val telemetryService: CrashTelemetryService = mock()

        val reporter = spy(
            CrashReporter(
                context = testContext,
                services = listOf(service),
                telemetryServices = listOf(telemetryService),
                shouldPrompt = CrashReporter.Prompt.NEVER,
                scope = scope,
            ).install(testContext),
        )

        val crash: Crash.UncaughtExceptionCrash = createUncaughtExceptionCrash()

        reporter.onCrash(testContext, crash)

        verify(reporter).sendCrashTelemetry(testContext, crash)
        verify(reporter).sendCrashReport(testContext, crash)
        verify(reporter, never()).showPrompt(any(), eq(crash))
    }

    @Test
    fun `CrashReporter will show prompt if setup with Prompt-ALWAYS`() {
        val service: CrashReporterService = mock()
        val telemetryService: CrashTelemetryService = mock()

        val reporter = spy(
            CrashReporter(
                context = testContext,
                services = listOf(service),
                telemetryServices = listOf(telemetryService),
                shouldPrompt = CrashReporter.Prompt.ALWAYS,
                scope = scope,
            ).install(testContext),
        )

        val crash: Crash.UncaughtExceptionCrash = createUncaughtExceptionCrash()

        reporter.onCrash(testContext, crash)

        verify(reporter).sendCrashTelemetry(testContext, crash)
        verify(reporter, never()).sendCrashReport(testContext, crash)
        verify(reporter).showPrompt(any(), eq(crash))
    }

    @Test
    fun `CrashReporter will submit report immediately for non native crash and with setup Prompt-ONLY_NATIVE_CRASH`() {
        val service: CrashReporterService = mock()
        val telemetryService: CrashTelemetryService = mock()

        val reporter = spy(
            CrashReporter(
                context = testContext,
                services = listOf(service),
                telemetryServices = listOf(telemetryService),
                shouldPrompt = CrashReporter.Prompt.ONLY_NATIVE_CRASH,
                scope = scope,
            ).install(testContext),
        )

        val crash: Crash.UncaughtExceptionCrash = createUncaughtExceptionCrash()

        reporter.onCrash(testContext, crash)

        verify(reporter).sendCrashTelemetry(testContext, crash)
        verify(reporter).sendCrashReport(testContext, crash)
        verify(reporter, never()).showPrompt(any(), eq(crash))
    }

    @Test
    fun `CrashReporter will show prompt for main process native crash and with setup Prompt-ONLY_NATIVE_CRASH`() {
        val service: CrashReporterService = mock()
        val telemetryService: CrashTelemetryService = mock()

        val reporter = spy(
            CrashReporter(
                context = testContext,
                services = listOf(service),
                telemetryServices = listOf(telemetryService),
                shouldPrompt = CrashReporter.Prompt.ONLY_NATIVE_CRASH,
                scope = scope,
            ).install(testContext),
        )

        val crash = Crash.NativeCodeCrash(
            0,
            "dump.path",
            true,
            "extras.path",
            processType = Crash.NativeCodeCrash.PROCESS_TYPE_MAIN,
            breadcrumbs = arrayListOf(),
        )

        reporter.onCrash(testContext, crash)

        verify(reporter).sendCrashTelemetry(testContext, crash)
        verify(reporter).showPrompt(any(), eq(crash))
        verify(reporter, never()).sendCrashReport(testContext, crash)
        verify(service, never()).report(crash)
    }

    @Test
    fun `CrashReporter will submit crash telemetry even if crash report requires prompt`() {
        val service: CrashReporterService = mock()
        val telemetryService: CrashTelemetryService = mock()

        val reporter = spy(
            CrashReporter(
                context = testContext,
                services = listOf(service),
                telemetryServices = listOf(telemetryService),
                shouldPrompt = CrashReporter.Prompt.ALWAYS,
            ).install(testContext),
        )

        val crash: Crash.UncaughtExceptionCrash = createUncaughtExceptionCrash()

        reporter.onCrash(testContext, crash)

        verify(reporter).sendCrashTelemetry(testContext, crash)
        verify(reporter, never()).sendCrashReport(testContext, crash)
        verify(reporter).showPrompt(any(), eq(crash))
    }

    @Test
    fun `CrashReporter will not prompt the user if there is no crash services`() {
        val telemetryService: CrashTelemetryService = mock()

        val reporter = spy(
            CrashReporter(
                context = testContext,
                telemetryServices = listOf(telemetryService),
                shouldPrompt = CrashReporter.Prompt.ALWAYS,
            ).install(testContext),
        )

        val crash: Crash.UncaughtExceptionCrash = createUncaughtExceptionCrash()

        reporter.onCrash(testContext, crash)

        verify(reporter).sendCrashTelemetry(testContext, crash)
        verify(reporter, never()).sendCrashReport(testContext, crash)
        verify(reporter, never()).showPrompt(any(), eq(crash))
    }

    @Test
    fun `CrashReporter will not send crash telemetry if there is no telemetry service`() {
        val service: CrashReporterService = mock()

        val reporter = spy(
            CrashReporter(
                context = testContext,
                services = listOf(service),
                shouldPrompt = CrashReporter.Prompt.ALWAYS,
            ).install(testContext),
        )

        val crash: Crash.UncaughtExceptionCrash = createUncaughtExceptionCrash()

        reporter.onCrash(testContext, crash)

        verify(reporter, never()).sendCrashTelemetry(testContext, crash)
        verify(reporter).showPrompt(any(), eq(crash))
    }

    @Test
    fun `Calling install() with no crash services or telemetry crash services will throw exception`() {
        var exceptionThrown = false

        try {
            CrashReporter(
                context = testContext,
                shouldPrompt = CrashReporter.Prompt.ALWAYS,
            ).install(testContext)
        } catch (e: IllegalArgumentException) {
            exceptionThrown = true
        }

        assert(exceptionThrown)
    }

    @Test
    fun `Calling install() with at least one crash service or telemetry crash service will not throw exception`() {
        var exceptionThrown = false

        try {
            CrashReporter(
                context = testContext,
                services = listOf(mock()),
            ).install(testContext)
        } catch (e: IllegalArgumentException) {
            exceptionThrown = true
        }
        assert(!exceptionThrown)

        try {
            CrashReporter(
                context = testContext,
                telemetryServices = listOf(mock()),
            ).install(testContext)
        } catch (e: IllegalArgumentException) {
            exceptionThrown = true
        }
        assert(!exceptionThrown)
    }

    @Test
    fun `CrashReporter is enabled by default`() {
        val reporter = spy(
            CrashReporter(
                context = testContext,
                services = listOf(mock()),
                shouldPrompt = CrashReporter.Prompt.ONLY_NATIVE_CRASH,
            ).install(testContext),
        )

        assertTrue(reporter.enabled)
    }

    @Test
    fun `CrashReporter will not prompt and not submit report if not enabled`() {
        val service: CrashReporterService = mock()

        val reporter = spy(
            CrashReporter(
                context = testContext,
                services = listOf(service),
                shouldPrompt = CrashReporter.Prompt.ALWAYS,
                scope = scope,
            ).install(testContext),
        )

        reporter.enabled = false

        val crash: Crash.UncaughtExceptionCrash = mock()
        reporter.onCrash(testContext, crash)

        verify(reporter, never()).sendCrashReport(testContext, crash)
        verify(reporter, never()).sendCrashTelemetry(testContext, crash)
        verify(reporter, never()).showPrompt(any(), eq(crash))

        verify(service, never()).report(crash)
    }

    @Test
    fun `CrashReporter sends telemetry`() {
        val crash = createUncaughtExceptionCrash()

        val service = mock<CrashReporterService>()
        val reporter = spy(
            CrashReporter(
                context = testContext,
                services = listOf(service),
                shouldPrompt = CrashReporter.Prompt.NEVER,
                scope = scope,
            ).install(testContext),
        )

        reporter.onCrash(testContext, crash)
        verify(reporter, never()).sendCrashTelemetry(testContext, crash)
    }

    @Test
    fun `CrashReporter forwards uncaught exception crashes to service`() {
        var exceptionCrash = false

        val service = object : CrashReporterService {
            override val id: String = "test"

            override val name: String = "TestReporter"

            override fun createCrashReportUrl(identifier: String): String? = null

            override fun report(crash: Crash.UncaughtExceptionCrash): String? {
                exceptionCrash = true
                return null
            }

            override fun report(crash: Crash.NativeCodeCrash): String? = null

            override fun report(throwable: Throwable, breadcrumbs: ArrayList<Breadcrumb>): String? = null
        }

        val reporter = spy(
            CrashReporter(
                context = testContext,
                services = listOf(service),
                shouldPrompt = CrashReporter.Prompt.NEVER,
            ).install(testContext),
        )

        reporter.submitReport(
            Crash.UncaughtExceptionCrash(0, RuntimeException(), arrayListOf()),
        ).joinBlocking()
        assertTrue(exceptionCrash)
    }

    @Test
    fun `CrashReporter forwards native crashes to service`() {
        var nativeCrash = false

        val service = object : CrashReporterService {
            override val id: String = "test"

            override val name: String = "TestReporter"

            override fun createCrashReportUrl(identifier: String): String? = null

            override fun report(crash: Crash.UncaughtExceptionCrash): String? = null

            override fun report(crash: Crash.NativeCodeCrash): String? {
                nativeCrash = true
                return null
            }

            override fun report(throwable: Throwable, breadcrumbs: ArrayList<Breadcrumb>): String? = null
        }

        val reporter = spy(
            CrashReporter(
                context = testContext,
                services = listOf(service),
                shouldPrompt = CrashReporter.Prompt.NEVER,
            ).install(testContext),
        )

        reporter.submitReport(
            Crash.NativeCodeCrash(0, "", true, "", Crash.NativeCodeCrash.PROCESS_TYPE_FOREGROUND_CHILD, arrayListOf()),
        ).joinBlocking()
        assertTrue(nativeCrash)
    }

    @Test
    fun `CrashReporter forwards caught exception crashes to service`() {
        val testMessage = "test_Message"
        val testData = hashMapOf("1" to "one", "2" to "two")
        val testCategory = "testing_category"
        val testLevel = Breadcrumb.Level.CRITICAL
        val testType = Breadcrumb.Type.USER
        var exceptionCrash = false
        var exceptionThrowable: Throwable? = null
        var exceptionBreadcrumb: ArrayList<Breadcrumb>? = null
        val service = object : CrashReporterService {
            override val id: String = "test"

            override val name: String = "TestReporter"

            override fun createCrashReportUrl(identifier: String): String? = null

            override fun report(crash: Crash.UncaughtExceptionCrash): String? = null

            override fun report(crash: Crash.NativeCodeCrash): String? = null

            override fun report(throwable: Throwable, breadcrumbs: ArrayList<Breadcrumb>): String? {
                exceptionCrash = true
                exceptionThrowable = throwable
                exceptionBreadcrumb = breadcrumbs
                return null
            }
        }

        val reporter = spy(
            CrashReporter(
                context = testContext,
                services = listOf(service),
                shouldPrompt = CrashReporter.Prompt.NEVER,
            ).install(testContext),
        )

        val throwable = RuntimeException()
        val breadcrumb = Breadcrumb(
            testMessage,
            testData,
            testCategory,
            testLevel,
            testType,
        )
        reporter.recordCrashBreadcrumb(breadcrumb)

        reporter.submitCaughtException(throwable).joinBlocking()

        assertTrue(exceptionCrash)
        assert(exceptionThrowable == throwable)
        assert(exceptionBreadcrumb?.get(0) == breadcrumb)
    }

    @Test
    fun `Caught exception with no stack trace should be reported as CrashReporterException`() {
        val testMessage = "test_Message"
        val testData = hashMapOf("1" to "one", "2" to "two")
        val testCategory = "testing_category"
        val testLevel = Breadcrumb.Level.CRITICAL
        val testType = Breadcrumb.Type.USER
        var exceptionCrash = false
        var exceptionThrowable: Throwable? = null
        var exceptionBreadcrumb: ArrayList<Breadcrumb>? = null
        val service = object : CrashReporterService {
            override val id: String = "test"

            override val name: String = "TestReporter"

            override fun createCrashReportUrl(identifier: String): String? = null

            override fun report(crash: Crash.UncaughtExceptionCrash): String? = null

            override fun report(crash: Crash.NativeCodeCrash): String? = null

            override fun report(throwable: Throwable, breadcrumbs: ArrayList<Breadcrumb>): String? {
                exceptionCrash = true
                exceptionThrowable = throwable
                exceptionBreadcrumb = breadcrumbs
                return null
            }
        }

        val reporter = spy(
            CrashReporter(
                context = testContext,
                services = listOf(service),
                shouldPrompt = CrashReporter.Prompt.NEVER,
            ).install(testContext),
        )

        val throwable = RuntimeException()
        throwable.stackTrace = emptyArray()
        val breadcrumb = Breadcrumb(
            testMessage,
            testData,
            testCategory,
            testLevel,
            testType,
        )
        reporter.recordCrashBreadcrumb(breadcrumb)

        reporter.submitCaughtException(throwable).joinBlocking()

        assertTrue(exceptionCrash)
        assert(exceptionThrowable is CrashReporterException.UnexpectedlyMissingStacktrace)
        assert(exceptionThrowable?.cause is java.lang.RuntimeException)
        assertEquals(exceptionBreadcrumb?.get(0), breadcrumb)
    }

    @Test
    fun `CrashReporter forwards native crashes to telemetry service`() {
        var nativeCrash = false

        val telemetryService = object : CrashTelemetryService {
            override fun record(crash: Crash.UncaughtExceptionCrash) = Unit

            override fun record(crash: Crash.NativeCodeCrash) {
                nativeCrash = true
            }

            override fun record(throwable: Throwable) = Unit
        }

        val reporter = spy(
            CrashReporter(
                context = testContext,
                telemetryServices = listOf(telemetryService),
                shouldPrompt = CrashReporter.Prompt.NEVER,
            ).install(testContext),
        )

        reporter.submitCrashTelemetry(
            Crash.NativeCodeCrash(0, "", true, "", Crash.NativeCodeCrash.PROCESS_TYPE_FOREGROUND_CHILD, arrayListOf()),
        ).joinBlocking()
        assertTrue(nativeCrash)
    }

    @Test
    fun `Internal reference is set after calling install`() {
        expectException(IllegalStateException::class) {
            CrashReporter.requireInstance
        }

        val reporter = CrashReporter(
            context = testContext,
            services = listOf(mock()),
        )

        expectException(IllegalStateException::class) {
            CrashReporter.requireInstance
        }

        reporter.install(testContext)

        assertNotNull(CrashReporter.requireInstance)
    }

    @Test
    fun `CrashReporter invokes PendingIntent if provided for foreground child process crashes`() {
        val context = Robolectric.buildActivity(Activity::class.java).setup().get()

        val intent = Intent("action")
        val pendingIntent = spy(PendingIntent.getActivity(context, 0, intent, 0))

        val reporter = CrashReporter(
            context = testContext,
            shouldPrompt = CrashReporter.Prompt.ALWAYS,
            services = listOf(mock()),
            nonFatalCrashIntent = pendingIntent,
        ).install(context)

        val nativeCrash = Crash.NativeCodeCrash(
            0,
            "dump.path",
            true,
            "extras.path",
            processType = Crash.NativeCodeCrash.PROCESS_TYPE_FOREGROUND_CHILD,
            breadcrumbs = arrayListOf(),
        )
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
        assertEquals(Crash.NativeCodeCrash.PROCESS_TYPE_FOREGROUND_CHILD, receivedCrash.processType)
    }

    @Test
    fun `CrashReporter does not invoke PendingIntent if provided for main process crashes`() {
        val context = Robolectric.buildActivity(Activity::class.java).setup().get()

        val intent = Intent("action")
        val pendingIntent = spy(PendingIntent.getActivity(context, 0, intent, 0))

        val reporter = CrashReporter(
            context = testContext,
            shouldPrompt = CrashReporter.Prompt.ALWAYS,
            services = listOf(mock()),
            nonFatalCrashIntent = pendingIntent,
        ).install(context)

        val nativeCrash = Crash.NativeCodeCrash(
            0,
            "dump.path",
            true,
            "extras.path",
            processType = Crash.NativeCodeCrash.PROCESS_TYPE_MAIN,
            breadcrumbs = arrayListOf(),
        )
        reporter.onCrash(context, nativeCrash)

        verify(pendingIntent, never()).send(eq(context), eq(0), any())
    }

    @Test
    fun `CrashReporter does not invoke PendingIntent if provided for background child process crashes`() {
        val context = Robolectric.buildActivity(Activity::class.java).setup().get()

        val intent = Intent("action")
        val pendingIntent = spy(PendingIntent.getActivity(context, 0, intent, 0))

        val reporter = CrashReporter(
            context = testContext,
            shouldPrompt = CrashReporter.Prompt.ALWAYS,
            services = listOf(mock()),
            nonFatalCrashIntent = pendingIntent,
        ).install(context)

        val nativeCrash = Crash.NativeCodeCrash(
            0,
            "dump.path",
            true,
            "extras.path",
            processType = Crash.NativeCodeCrash.PROCESS_TYPE_BACKGROUND_CHILD,
            breadcrumbs = arrayListOf(),
        )
        reporter.onCrash(context, nativeCrash)

        verify(pendingIntent, never()).send(eq(context), eq(0), any())
    }

    @Test
    fun `CrashReporter sends telemetry but don't send native crash if the crash is in foreground child process and nonFatalPendingIntent is not null`() {
        val service: CrashReporterService = mock()
        val telemetryService: CrashTelemetryService = mock()

        val reporter = spy(
            CrashReporter(
                context = testContext,
                services = listOf(service),
                telemetryServices = listOf(telemetryService),
                shouldPrompt = CrashReporter.Prompt.NEVER,
                nonFatalCrashIntent = mock(),
                scope = scope,
            ).install(testContext),
        )

        val nativeCrash = Crash.NativeCodeCrash(
            0,
            "dump.path",
            true,
            "extras.path",
            processType = Crash.NativeCodeCrash.PROCESS_TYPE_FOREGROUND_CHILD,
            breadcrumbs = arrayListOf(),
        )
        reporter.onCrash(testContext, nativeCrash)

        verify(reporter, never()).sendCrashReport(testContext, nativeCrash)
        verify(reporter, times(1)).sendCrashTelemetry(testContext, nativeCrash)
        verify(reporter, never()).showPrompt(any(), eq(nativeCrash))
    }

    @Test
    fun `CrashReporter sends telemetry and crash if the crash is in foreground child process and nonFatalPendingIntent is null`() {
        val service: CrashReporterService = mock()
        val telemetryService: CrashTelemetryService = mock()

        val reporter = spy(
            CrashReporter(
                context = testContext,
                services = listOf(service),
                telemetryServices = listOf(telemetryService),
                shouldPrompt = CrashReporter.Prompt.NEVER,
                scope = scope,
            ).install(testContext),
        )

        val nativeCrash = Crash.NativeCodeCrash(
            0,
            "dump.path",
            true,
            "extras.path",
            processType = Crash.NativeCodeCrash.PROCESS_TYPE_FOREGROUND_CHILD,
            breadcrumbs = arrayListOf(),
        )
        reporter.onCrash(testContext, nativeCrash)

        verify(reporter, times(1)).sendCrashReport(testContext, nativeCrash)
        verify(reporter, times(1)).sendCrashTelemetry(testContext, nativeCrash)
        verify(reporter, never()).showPrompt(any(), eq(nativeCrash))
    }

    @Test
    fun `CrashReporter instance writes are visible across threads`() {
        val instanceField = CrashReporter::class.java.getDeclaredField("instance")
        assertTrue(Modifier.isVolatile(instanceField.modifiers))
    }

    @Test
    fun `Breadcrumbs stores only max number of breadcrumbs`() {
        val testMessage = "test_Message"
        val testData = hashMapOf("1" to "one", "2" to "two")
        val testCategory = "testing_category"
        val testLevel = Breadcrumb.Level.CRITICAL
        val testType = Breadcrumb.Type.USER

        var crashReporter = CrashReporter(
            context = testContext,
            services = listOf(mock()),
            maxBreadCrumbs = 5,
        )

        repeat(10) {
            crashReporter.recordCrashBreadcrumb(Breadcrumb(testMessage, testData, testCategory, testLevel, testType))
        }
        assertEquals(crashReporter.crashBreadcrumbs.size, 5)

        crashReporter = CrashReporter(
            context = testContext,
            services = listOf(mock()),
            maxBreadCrumbs = 5,
        )
        repeat(15) {
            crashReporter.recordCrashBreadcrumb(Breadcrumb(testMessage, testData, testCategory, testLevel, testType))
        }
        assertEquals(crashReporter.crashBreadcrumbs.size, 5)
    }

    @Test
    fun `Breadcrumb priority queue stores the latest breadcrumbs`() {
        val testMessage = "test_Message"
        val testData = hashMapOf("1" to "one", "2" to "two")
        val testCategory = "testing_category"
        val testType = Breadcrumb.Type.USER
        val maxNum = 10

        var crashReporter = CrashReporter(
            context = testContext,
            services = listOf(mock()),
            maxBreadCrumbs = maxNum,
        )

        repeat(maxNum) {
            crashReporter.recordCrashBreadcrumb(
                Breadcrumb(testMessage, testData, testCategory, Breadcrumb.Level.CRITICAL, testType),
            )
            sleep(10) /* make sure time elapsed */
        }

        for (i in 0 until maxNum) {
            assertEquals(crashReporter.crashBreadcrumbs.elementAt(i).level, Breadcrumb.Level.CRITICAL)
        }

        var time = crashReporter.crashBreadcrumbs[0].date
        for (i in 1 until crashReporter.crashBreadcrumbs.size) {
            assertTrue(time.before(crashReporter.crashBreadcrumbs[i].date))
            time = crashReporter.crashBreadcrumbs[i].date
        }

        repeat(maxNum) {
            crashReporter.recordCrashBreadcrumb(
                Breadcrumb(testMessage, testData, testCategory, Breadcrumb.Level.DEBUG, testType),
            )
            sleep(10) /* make sure time elapsed */
        }

        for (i in 0 until maxNum) {
            assertEquals(crashReporter.crashBreadcrumbs.elementAt(i).level, Breadcrumb.Level.DEBUG)
        }

        time = crashReporter.crashBreadcrumbs[0].date
        for (i in 1 until crashReporter.crashBreadcrumbs.size) {
            assertTrue(time.before(crashReporter.crashBreadcrumbs[i].date))
            time = crashReporter.crashBreadcrumbs[i].date
        }
    }

    @Test
    fun `Breadcrumb priority queue output list result is sorted by time`() {
        val testMessage = "test_Message"
        val testData = hashMapOf("1" to "one", "2" to "two")
        val testCategory = "testing_category"
        val testType = Breadcrumb.Type.USER
        val maxNum = 10

        var crashReporter = CrashReporter(
            context = testContext,
            services = listOf(mock()),
            maxBreadCrumbs = 5,
        )

        repeat(maxNum) {
            crashReporter.recordCrashBreadcrumb(
                Breadcrumb(testMessage, testData, testCategory, Breadcrumb.Level.DEBUG, testType),
            )
            sleep(10) /* make sure time elapsed */
        }

        var time = crashReporter.crashBreadcrumbs[0].date
        for (i in 1 until crashReporter.crashBreadcrumbs.size) {
            assertTrue(time.before(crashReporter.crashBreadcrumbs[i].date))
            time = crashReporter.crashBreadcrumbs[i].date
        }

        repeat(maxNum / 2) {
            crashReporter.recordCrashBreadcrumb(
                Breadcrumb(testMessage, testData, testCategory, Breadcrumb.Level.INFO, testType),
            )
            sleep(10) /* make sure time elapsed */
        }

        time = crashReporter.crashBreadcrumbs[0].date
        for (i in 1 until crashReporter.crashBreadcrumbs.size) {
            assertTrue(time.before(crashReporter.crashBreadcrumbs[i].date))
            time = crashReporter.crashBreadcrumbs[i].date
        }
    }
}

private fun createUncaughtExceptionCrash(): Crash.UncaughtExceptionCrash {
    return Crash.UncaughtExceptionCrash(
        0,
        RuntimeException(),
        ArrayList(),
    )
}
