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
import java.io.File
import java.util.Calendar
import java.util.Date
import java.util.GregorianCalendar
import mozilla.components.lib.crash.GleanMetrics.Crash as GleanCrash
import mozilla.components.lib.crash.GleanMetrics.Pings as GleanPings

@RunWith(AndroidJUnit4::class)
class GleanCrashReporterServiceTest {
    private val context: Context
        get() = ApplicationProvider.getApplicationContext()

    @get:Rule
    val gleanRule = GleanTestRule(context)

    private fun crashCountJson(key: String): String = "{\"type\":\"count\",\"label\":\"$key\"}"

    private fun crashPingJson(uptime: Long, type: String, time: Long, startup: Boolean): String =
        "{\"type\":\"ping\",\"uptimeNanos\":$uptime,\"processType\":\"$type\"," +
            "\"timeMillis\":$time,\"startup\":$startup,\"reason\":\"crash\"}"

    private fun exceptionPingJson(uptime: Long, time: Long, startup: Boolean): String =
        "{\"type\":\"ping\",\"uptimeNanos\":$uptime,\"processType\":\"main\"," +
            "\"timeMillis\":$time,\"startup\":$startup,\"reason\":\"crash\",\"cause\":\"java_exception\"}"

    @Test
    fun `GleanCrashReporterService records all crash types`() {
        val crashTypes = hashMapOf(
            GleanCrashReporterService.MAIN_PROCESS_NATIVE_CODE_CRASH_KEY to Crash.NativeCodeCrash(
                0,
                "",
                true,
                "",
                Crash.NativeCodeCrash.PROCESS_TYPE_MAIN,
                arrayListOf(),
            ),
            GleanCrashReporterService.FOREGROUND_CHILD_PROCESS_NATIVE_CODE_CRASH_KEY to Crash.NativeCodeCrash(
                0,
                "",
                true,
                "",
                Crash.NativeCodeCrash.PROCESS_TYPE_FOREGROUND_CHILD,
                arrayListOf(),
            ),
            GleanCrashReporterService.BACKGROUND_CHILD_PROCESS_NATIVE_CODE_CRASH_KEY to Crash.NativeCodeCrash(
                0,
                "",
                true,
                "",
                Crash.NativeCodeCrash.PROCESS_TYPE_BACKGROUND_CHILD,
                arrayListOf(),
            ),
            GleanCrashReporterService.UNCAUGHT_EXCEPTION_KEY to Crash.UncaughtExceptionCrash(
                0,
                RuntimeException("Test"),
                arrayListOf(),
            ),
            GleanCrashReporterService.CAUGHT_EXCEPTION_KEY to RuntimeException("Test"),
        )

        for ((type, crash) in crashTypes) {
            // Because of how Glean is implemented, it can potentially persist information between
            // tests or even between test classes, so we compensate by capturing the initial value
            // to compare to.
            val initialValue = try {
                CrashMetrics.crashCount[type].testGetValue()!!
            } catch (e: NullPointerException) {
                0
            }

            run {
                val service = spy(GleanCrashReporterService(context))

                assertFalse("No previous persisted crashes must exist", service.file.exists())

                when (crash) {
                    is Crash.NativeCodeCrash -> service.record(crash)
                    is Crash.UncaughtExceptionCrash -> service.record(crash)
                    is Throwable -> service.record(crash)
                }

                assertTrue("Persistence file must exist", service.file.exists())
                val lines = service.file.readLines()
                assertEquals(
                    "Must be $type",
                    crashCountJson(type),
                    lines.first(),
                )
            }

            // Initialize a fresh GleanCrashReporterService and ensure metrics are recorded in Glean
            run {
                GleanCrashReporterService(context)

                assertEquals(
                    "Glean must record correct value",
                    1,
                    CrashMetrics.crashCount[type].testGetValue()!! - initialValue,
                )
            }
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

            val uncaughtExceptionCrash =
                Crash.UncaughtExceptionCrash(0, RuntimeException("Test"), arrayListOf())
            val mainProcessNativeCodeCrash = Crash.NativeCodeCrash(
                0,
                "",
                true,
                "",
                Crash.NativeCodeCrash.PROCESS_TYPE_MAIN,
                arrayListOf(),
            )
            val foregroundChildProcessNativeCodeCrash = Crash.NativeCodeCrash(
                0,
                "",
                true,
                "",
                Crash.NativeCodeCrash.PROCESS_TYPE_FOREGROUND_CHILD,
                arrayListOf(),
            )
            val backgroundChildProcessNativeCodeCrash = Crash.NativeCodeCrash(
                0,
                "",
                true,
                "",
                Crash.NativeCodeCrash.PROCESS_TYPE_BACKGROUND_CHILD,
                arrayListOf(),
            )

            // Record some crashes
            service.record(uncaughtExceptionCrash)
            service.record(mainProcessNativeCodeCrash)
            service.record(uncaughtExceptionCrash)
            service.record(foregroundChildProcessNativeCodeCrash)
            service.record(backgroundChildProcessNativeCodeCrash)

            // Make sure the file exists
            assertTrue("Persistence file must exist", service.file.exists())

            // Get the file lines
            val lines = service.file.readLines().iterator()
            assertEquals(
                "element must be uncaught exception",
                crashCountJson(GleanCrashReporterService.UNCAUGHT_EXCEPTION_KEY),
                lines.next(),
            )
            assertEquals(
                "element must be uncaught exception ping",
                exceptionPingJson(0, 0, false),
                lines.next(),
            )
            assertEquals(
                "element must be main process native code crash",
                crashCountJson(GleanCrashReporterService.MAIN_PROCESS_NATIVE_CODE_CRASH_KEY),
                lines.next(),
            )
            assertEquals(
                "element must be main process crash ping",
                crashPingJson(0, "main", 0, false),
                lines.next(),
            )
            assertEquals(
                "element must be uncaught exception",
                crashCountJson(GleanCrashReporterService.UNCAUGHT_EXCEPTION_KEY),
                lines.next(), // skip crash ping line in this test
            )
            assertEquals(
                "element must be uncaught exception ping",
                exceptionPingJson(0, 0, false),
                lines.next(),
            )
            assertEquals(
                "element must be foreground child process native code crash",
                crashCountJson(GleanCrashReporterService.FOREGROUND_CHILD_PROCESS_NATIVE_CODE_CRASH_KEY),
                lines.next(),
            )
            assertEquals(
                "element must be foreground process crash ping",
                crashPingJson(0, "content", 0, false),
                lines.next(),
            )
            assertEquals(
                "element must be background child process native code crash",
                crashCountJson(GleanCrashReporterService.BACKGROUND_CHILD_PROCESS_NATIVE_CODE_CRASH_KEY),
                lines.next(), // skip crash ping line
            )
            assertEquals(
                "element must be background process crash ping",
                crashPingJson(0, "utility", 0, false),
                lines.next(),
            )
            assertFalse(lines.hasNext())
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
        val file =
            spy(File(context.applicationInfo.dataDir, GleanCrashReporterService.CRASH_FILE_NAME))
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
            CrashMetrics.crashCount[GleanCrashReporterService.UNCAUGHT_EXCEPTION_KEY].testGetValue()!!
        } catch (e: NullPointerException) {
            0
        }

        run {
            val service = spy(GleanCrashReporterService(context))

            assertFalse("No previous persisted crashes must exist", service.file.exists())

            val crash = Crash.UncaughtExceptionCrash(
                0,
                RuntimeException("Test"),
                arrayListOf(),
            )
            service.record(crash)

            assertTrue("Persistence file must exist", service.file.exists())

            // Add bad data
            service.file.appendText("bad data in here\n")

            val lines = service.file.readLines()
            assertEquals(
                "must be native code crash",
                "{\"type\":\"count\",\"label\":\"${GleanCrashReporterService.UNCAUGHT_EXCEPTION_KEY}\"}",
                lines.first(),
            )
            assertEquals(
                "must be uncaught exception ping",
                exceptionPingJson(0, 0, false),
                lines[1],
            )
            assertEquals("bad data in here", lines[2])
        }

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
    fun `GleanCrashReporterService sends crash pings`() {
        val service = spy(GleanCrashReporterService(context))

        val crash = Crash.NativeCodeCrash(
            12340000,
            "",
            true,
            "",
            Crash.NativeCodeCrash.PROCESS_TYPE_MAIN,
            arrayListOf(),
        )

        service.record(crash)

        assertTrue("Persistence file must exist", service.file.exists())

        val lines = service.file.readLines()
        assertEquals(
            "First element must be main process native code crash",
            crashCountJson(GleanCrashReporterService.MAIN_PROCESS_NATIVE_CODE_CRASH_KEY),
            lines[0],
        )
        assertEquals(
            "Second element must be main process crash ping",
            crashPingJson(0, "main", 12340000, false),
            lines[1],
        )

        run {
            var pingReceived = false
            GleanPings.crash.testBeforeNextSubmit { _ ->
                val date = GregorianCalendar().apply {
                    time = Date(12340000)
                }
                date.set(Calendar.SECOND, 0)
                date.set(Calendar.MILLISECOND, 0)
                assertEquals(date.time, GleanCrash.time.testGetValue())
                assertEquals(0L, GleanCrash.uptime.testGetValue())
                assertEquals("main", GleanCrash.processType.testGetValue())
                assertEquals(false, GleanCrash.startup.testGetValue())
                assertEquals("os_fault", GleanCrash.cause.testGetValue())
                pingReceived = true
            }

            GleanCrashReporterService(context)
            assertTrue("Expected ping to be sent", pingReceived)
        }
    }

    @Test
    fun `GleanCrashReporterService serialized pings are forward compatible`() {
        val service = spy(GleanCrashReporterService(context))

        // Original ping fields (no e.g. `cause` field)
        service.file.appendText(
            "{\"type\":\"ping\",\"uptimeNanos\":0,\"processType\":\"main\"," +
                "\"timeMillis\":0,\"startup\":false,\"reason\":\"crash\"}\n",
        )

        assertTrue("Persistence file must exist", service.file.exists())

        run {
            var pingReceived = false
            GleanPings.crash.testBeforeNextSubmit { _ ->
                val date = GregorianCalendar().apply {
                    time = Date(0)
                }
                date.set(Calendar.SECOND, 0)
                date.set(Calendar.MILLISECOND, 0)
                assertEquals(date.time, GleanCrash.time.testGetValue())
                assertEquals(0L, GleanCrash.uptime.testGetValue())
                assertEquals("main", GleanCrash.processType.testGetValue())
                assertEquals(false, GleanCrash.startup.testGetValue())
                assertEquals("os_fault", GleanCrash.cause.testGetValue())
                pingReceived = true
            }

            GleanCrashReporterService(context)
            assertTrue("Expected ping to be sent", pingReceived)
        }
    }
}
