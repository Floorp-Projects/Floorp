/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.crash.service

import android.content.Context
import androidx.test.core.app.ApplicationProvider
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.lib.crash.Crash
import mozilla.components.lib.crash.GleanMetrics.CrashMetrics
import mozilla.components.service.glean.testing.GleanTestRule
import mozilla.components.support.test.whenever
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.spy
import org.mockito.Mockito.verify
import java.io.File

@RunWith(AndroidJUnit4::class)
class GleanCrashReporterServiceTest {
    private val context: Context
        get() = ApplicationProvider.getApplicationContext()

    @get:Rule
    val gleanRule = GleanTestRule(context)

    @Test
    fun `GleanCrashReporterService records main process native code crashes`() {
        // Because of how Glean is implemented, it can potentially persist information between
        // tests or even between test classes, so we compensate by capturing the initial value
        // to compare to.
        val initialValue = try {
            CrashMetrics.crashCount[GleanCrashReporterService.MAIN_PROCESS_NATIVE_CODE_CRASH_KEY].testGetValue()!!
        } catch (e: NullPointerException) {
            0
        }

        run {
            val service = spy(GleanCrashReporterService(context))

            assertFalse("No previous persisted crashes must exist", service.file.exists())

            val crash = Crash.NativeCodeCrash(0, "", true, "", Crash.NativeCodeCrash.PROCESS_TYPE_MAIN, arrayListOf())
            service.record(crash)

            verify(service).record(crash)

            assertTrue("Persistence file must exist", service.file.exists())
            val lines = service.file.readLines()
            assertEquals(
                "Must be main process native code crash",
                GleanCrashReporterService.MAIN_PROCESS_NATIVE_CODE_CRASH_KEY,
                lines.first(),
            )
        }

        // Initialize a fresh GleanCrashReporterService and ensure metrics are recorded in Glean
        run {
            GleanCrashReporterService(context)

            assertEquals(
                "Glean must record correct value",
                1,
                CrashMetrics.crashCount[GleanCrashReporterService.MAIN_PROCESS_NATIVE_CODE_CRASH_KEY].testGetValue()!! - initialValue,
            )
        }
    }

    @Test
    fun `GleanCrashReporterService records foreground child process native code crashes`() {
        // Because of how Glean is implemented, it can potentially persist information between
        // tests or even between test classes, so we compensate by capturing the initial value
        // to compare to.
        val initialValue = try {
            CrashMetrics.crashCount[GleanCrashReporterService.FOREGROUND_CHILD_PROCESS_NATIVE_CODE_CRASH_KEY].testGetValue()!!
        } catch (e: NullPointerException) {
            0
        }

        run {
            val service = spy(GleanCrashReporterService(context))

            assertFalse("No previous persisted crashes must exist", service.file.exists())

            val crash = Crash.NativeCodeCrash(0, "", true, "", Crash.NativeCodeCrash.PROCESS_TYPE_FOREGROUND_CHILD, arrayListOf())
            service.record(crash)

            assertTrue("Persistence file must exist", service.file.exists())
            val lines = service.file.readLines()
            assertEquals(
                "Must be foreground child process native code crash",
                GleanCrashReporterService.FOREGROUND_CHILD_PROCESS_NATIVE_CODE_CRASH_KEY,
                lines.first(),
            )
        }

        // Initialize a fresh GleanCrashReporterService and ensure metrics are recorded in Glean
        run {
            GleanCrashReporterService(context)

            assertEquals(
                "Glean must record correct value",
                1,
                CrashMetrics.crashCount[GleanCrashReporterService.FOREGROUND_CHILD_PROCESS_NATIVE_CODE_CRASH_KEY].testGetValue()!! - initialValue,
            )
        }
    }

    @Test
    fun `GleanCrashReporterService records background child process native code crashes`() {
        // Because of how Glean is implemented, it can potentially persist information between
        // tests or even between test classes, so we compensate by capturing the initial value
        // to compare to.
        val initialValue = try {
            CrashMetrics.crashCount[GleanCrashReporterService.BACKGROUND_CHILD_PROCESS_NATIVE_CODE_CRASH_KEY].testGetValue()!!
        } catch (e: NullPointerException) {
            0
        }

        run {
            val service = spy(GleanCrashReporterService(context))

            assertFalse("No previous persisted crashes must exist", service.file.exists())

            val crash = Crash.NativeCodeCrash(0, "", true, "", Crash.NativeCodeCrash.PROCESS_TYPE_BACKGROUND_CHILD, arrayListOf())
            service.record(crash)

            assertTrue("Persistence file must exist", service.file.exists())
            val lines = service.file.readLines()
            assertEquals(
                "Must be background child process native code crash",
                GleanCrashReporterService.BACKGROUND_CHILD_PROCESS_NATIVE_CODE_CRASH_KEY,
                lines.first(),
            )
        }

        // Initialize a fresh GleanCrashReporterService and ensure metrics are recorded in Glean
        run {
            GleanCrashReporterService(context)

            assertEquals(
                "Glean must record correct value",
                1,
                CrashMetrics.crashCount[GleanCrashReporterService.BACKGROUND_CHILD_PROCESS_NATIVE_CODE_CRASH_KEY].testGetValue()!! - initialValue,
            )
        }
    }

    @Test
    fun `GleanCrashReporterService records uncaught exceptions`() {
        // Because of how Glean is implemented, it can potentially persist information between
        // tests or even between test classes, so we compensate by capturing the initial value
        // to compare to.
        val initialValue = try {
            CrashMetrics.crashCount[GleanCrashReporterService.UNCAUGHT_EXCEPTION_KEY].testGetValue()!!
        } catch (e: NullPointerException) {
            0
        }

        run {
            val service = spy(GleanCrashReporterService(context))

            assertFalse("No previous persisted crashes must exist", service.file.exists())

            val crash = Crash.UncaughtExceptionCrash(0, RuntimeException("Test"), arrayListOf())
            service.record(crash)

            assertTrue("Persistence file must exist", service.file.exists())
            val lines = service.file.readLines()
            assertEquals(
                "Must be uncaught exception",
                GleanCrashReporterService.UNCAUGHT_EXCEPTION_KEY,
                lines.first(),
            )
        }

        // Initialize a fresh GleanCrashReporterService and ensure metrics are recorded in Glean
        run {
            GleanCrashReporterService(context)

            assertEquals(
                "Glean must record correct value",
                1,
                CrashMetrics.crashCount[GleanCrashReporterService.UNCAUGHT_EXCEPTION_KEY].testGetValue()!! - initialValue,
            )
        }
    }

    @Test
    fun `GleanCrashReporterService records caught exceptions`() {
        // Because of how Glean is implemented, it can potentially persist information between
        // tests or even between test classes, so we compensate by capturing the initial value
        // to compare to.
        val initialValue = try {
            CrashMetrics.crashCount[GleanCrashReporterService.CAUGHT_EXCEPTION_KEY].testGetValue()!!
        } catch (e: NullPointerException) {
            0
        }

        run {
            val service = spy(GleanCrashReporterService(context))

            assertFalse("No previous persisted crashes must exist", service.file.exists())

            val throwable = RuntimeException("Test")
            service.record(throwable)

            assertTrue("Persistence file must exist", service.file.exists())
            val lines = service.file.readLines()
            assertEquals(
                "Must be caught exception",
                GleanCrashReporterService.CAUGHT_EXCEPTION_KEY,
                lines.first(),
            )
        }

        // Initialize a fresh GleanCrashReporterService and ensure metrics are recorded in Glean
        run {
            GleanCrashReporterService(context)

            assertEquals(
                "Glean must record correct value",
                1,
                CrashMetrics.crashCount[GleanCrashReporterService.CAUGHT_EXCEPTION_KEY].testGetValue()!! - initialValue,
            )
        }
    }

    @Test
    fun `GleanCrashReporterService correctly handles multiple crashes in a single file`() {
        val initialExceptionValue = try {
            CrashMetrics.crashCount[GleanCrashReporterService.UNCAUGHT_EXCEPTION_KEY].testGetValue()!!
        } catch (e: NullPointerException) {
            0
        }
        val initialMainProcessNativeCrashValue = try {
            CrashMetrics.crashCount[GleanCrashReporterService.MAIN_PROCESS_NATIVE_CODE_CRASH_KEY].testGetValue()!!
        } catch (e: NullPointerException) {
            0
        }

        val initialForegroundChildProcessNativeCrashValue = try {
            CrashMetrics.crashCount[GleanCrashReporterService.FOREGROUND_CHILD_PROCESS_NATIVE_CODE_CRASH_KEY].testGetValue()!!
        } catch (e: NullPointerException) {
            0
        }

        val initialBackgroundChildProcessNativeCrashValue = try {
            CrashMetrics.crashCount[GleanCrashReporterService.BACKGROUND_CHILD_PROCESS_NATIVE_CODE_CRASH_KEY].testGetValue()!!
        } catch (e: NullPointerException) {
            0
        }

        run {
            val service = spy(GleanCrashReporterService(context))

            assertFalse("No previous persisted crashes must exist", service.file.exists())

            val uncaughtExceptionCrash = Crash.UncaughtExceptionCrash(0, RuntimeException("Test"), arrayListOf())
            val mainProcessNativeCodeCrash = Crash.NativeCodeCrash(0, "", true, "", Crash.NativeCodeCrash.PROCESS_TYPE_MAIN, arrayListOf())
            val foregroundChildProcessNativeCodeCrash = Crash.NativeCodeCrash(0, "", true, "", Crash.NativeCodeCrash.PROCESS_TYPE_FOREGROUND_CHILD, arrayListOf())
            val backgroundChildProcessNativeCodeCrash = Crash.NativeCodeCrash(0, "", true, "", Crash.NativeCodeCrash.PROCESS_TYPE_BACKGROUND_CHILD, arrayListOf())

            // Record some crashes
            service.record(uncaughtExceptionCrash)
            service.record(mainProcessNativeCodeCrash)
            service.record(uncaughtExceptionCrash)
            service.record(foregroundChildProcessNativeCodeCrash)
            service.record(backgroundChildProcessNativeCodeCrash)

            // Make sure the file exists
            assertTrue("Persistence file must exist", service.file.exists())

            // Get the file lines
            val lines = service.file.readLines()
            assertEquals(5, lines.count())
            assertEquals(
                "First element must be uncaught exception",
                GleanCrashReporterService.UNCAUGHT_EXCEPTION_KEY,
                lines[0],
            )
            assertEquals(
                "Second element must be main process native code crash",
                GleanCrashReporterService.MAIN_PROCESS_NATIVE_CODE_CRASH_KEY,
                lines[1],
            )
            assertEquals(
                "Third element must be uncaught exception",
                GleanCrashReporterService.UNCAUGHT_EXCEPTION_KEY,
                lines[2],
            )
            assertEquals(
                "Fourth element must be foreground child process native code crash",
                GleanCrashReporterService.FOREGROUND_CHILD_PROCESS_NATIVE_CODE_CRASH_KEY,
                lines[3],
            )
            assertEquals(
                "Fifth element must be background child process native code crash",
                GleanCrashReporterService.BACKGROUND_CHILD_PROCESS_NATIVE_CODE_CRASH_KEY,
                lines[4],
            )
        }

        // Initialize a fresh GleanCrashReporterService and ensure metrics are recorded in Glean
        run {
            GleanCrashReporterService(context)

            assertEquals(
                "Glean must record correct value",
                2,
                CrashMetrics.crashCount[GleanCrashReporterService.UNCAUGHT_EXCEPTION_KEY].testGetValue()!! - initialExceptionValue,
            )
            assertEquals(
                "Glean must record correct value",
                1,
                CrashMetrics.crashCount[GleanCrashReporterService.MAIN_PROCESS_NATIVE_CODE_CRASH_KEY].testGetValue()!! - initialMainProcessNativeCrashValue,
            )
            assertEquals(
                "Glean must record correct value",
                1,
                CrashMetrics.crashCount[GleanCrashReporterService.FOREGROUND_CHILD_PROCESS_NATIVE_CODE_CRASH_KEY].testGetValue()!! - initialForegroundChildProcessNativeCrashValue,
            )
            assertEquals(
                "Glean must record correct value",
                1,
                CrashMetrics.crashCount[GleanCrashReporterService.BACKGROUND_CHILD_PROCESS_NATIVE_CODE_CRASH_KEY].testGetValue()!! - initialBackgroundChildProcessNativeCrashValue,
            )
        }
    }

    @Test
    fun `GleanCrashReporterService does not crash if it can't write to it's file`() {
        val file = spy(File(context.applicationInfo.dataDir, GleanCrashReporterService.CRASH_FILE_NAME))
        whenever(file.canWrite()).thenReturn(false)
        val service = spy(GleanCrashReporterService(context, file))

        assertFalse("No previous persisted crashes must exist", service.file.exists())

        val crash = Crash.UncaughtExceptionCrash(0, RuntimeException("Test"), arrayListOf())
        service.record(crash)

        assertTrue("Persistence file must exist", service.file.exists())
        val lines = service.file.readLines()
        assertEquals("Must be empty due to mocked write error", 0, lines.count())
    }

    @Test
    fun `GleanCrashReporterService does not crash if the persistent file is corrupted`() {
        // Because of how Glean is implemented, it can potentially persist information between
        // tests or even between test classes, so we compensate by capturing the initial value
        // to compare to.
        val initialValue = try {
            CrashMetrics.crashCount[GleanCrashReporterService.MAIN_PROCESS_NATIVE_CODE_CRASH_KEY].testGetValue()!!
        } catch (e: NullPointerException) {
            0
        }

        run {
            val service = spy(GleanCrashReporterService(context))

            assertFalse("No previous persisted crashes must exist", service.file.exists())

            val crash = Crash.NativeCodeCrash(0, "", true, "", Crash.NativeCodeCrash.PROCESS_TYPE_MAIN, arrayListOf())
            service.record(crash)

            assertTrue("Persistence file must exist", service.file.exists())

            // Add bad data
            service.file.appendText("bad data in here\n")

            val lines = service.file.readLines()
            assertEquals(
                "First must be native code crash",
                GleanCrashReporterService.MAIN_PROCESS_NATIVE_CODE_CRASH_KEY,
                lines.first(),
            )
            assertEquals("bad data in here", lines[1])
        }

        run {
            GleanCrashReporterService(context)

            assertEquals(
                "Glean must record correct value",
                1,
                CrashMetrics.crashCount[GleanCrashReporterService.MAIN_PROCESS_NATIVE_CODE_CRASH_KEY].testGetValue()!! - initialValue,
            )
        }
    }
}
