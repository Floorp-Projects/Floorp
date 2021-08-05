/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.rustlog

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.Job
import mozilla.appservices.rustlog.LogAdapterCannotEnable
import mozilla.appservices.rustlog.LogLevelFilter
import mozilla.components.concept.base.crash.Breadcrumb
import mozilla.components.concept.base.crash.CrashReporting
import mozilla.components.support.base.log.Log
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verifyNoInteractions

@RunWith(AndroidJUnit4::class)
class RustLogTest {
    @Test
    fun `basic RustLog interactions do not blow up`() {
        RustLog.enable()

        try {
            RustLog.enable()
            fail("can't enable RustLog more than once")
        } catch (e: LogAdapterCannotEnable) {}

        try {
            RustLog.enable(mock())
            fail("can't enable RustLog more than once")
        } catch (e: LogAdapterCannotEnable) {}

        RustLog.setMaxLevel(Log.Priority.DEBUG, false)
        RustLog.setMaxLevel(Log.Priority.DEBUG, true)
        RustLog.setMaxLevel(Log.Priority.INFO, false)
        RustLog.setMaxLevel(Log.Priority.INFO, true)
        RustLog.setMaxLevel(Log.Priority.WARN, false)
        RustLog.setMaxLevel(Log.Priority.WARN, true)
        RustLog.setMaxLevel(Log.Priority.ERROR, false)
        RustLog.setMaxLevel(Log.Priority.ERROR, true)

        RustLog.disable()
        RustLog.enable(mock())
    }

    @Test
    fun `maybeSendLogToCrashReporter log processing ignores low priority stuff`() {
        val crashReporter = mock<CrashReporting>()
        val onLog = CrashReporterOnLog(crashReporter)

        onLog(android.util.Log.VERBOSE, "sync15:multiple", "Stuff broke")
        verifyNoInteractions(crashReporter)

        onLog(android.util.Log.DEBUG, "sync15:multiple", "Stuff broke")
        verifyNoInteractions(crashReporter)

        onLog(android.util.Log.INFO, "sync15:multiple", "Stuff broke")
        verifyNoInteractions(crashReporter)

        onLog(android.util.Log.WARN, "sync15:multiple", "Stuff broke")
        verifyNoInteractions(crashReporter)

        onLog(android.util.Log.WARN, null, "Stuff broke")
        verifyNoInteractions(crashReporter)
    }

    @Test
    fun `maybeSendLogToCrashReporter log processing reports error level stuff`() {
        val crashReporter = TestCrashReporter()
        val onLog = CrashReporterOnLog(crashReporter)

        onLog(android.util.Log.ERROR, "sync15:multiple", "Stuff broke")
        crashReporter.assertLastException(1, "sync15:multiple - Stuff broke")

        onLog(android.util.Log.ERROR, null, "Something maybe broke")
        crashReporter.assertLastException(2, "null - Something maybe broke")
    }

    @Test
    fun `maybeSendLogToCrashReporter log processing ignores certain tags`() {
        val expectedIgnoredTags = listOf("viaduct::backend::ffi")
        val crashReporter = TestCrashReporter()
        val onLog = CrashReporterOnLog(crashReporter)

        expectedIgnoredTags.forEach { tag ->
            onLog(android.util.Log.ERROR, tag, "Stuff broke")
            assertTrue(crashReporter.exceptions.isEmpty())
        }

        // null tags are fine
        onLog(android.util.Log.ERROR, null, "Stuff broke")
        crashReporter.assertLastException(1, "null - Stuff broke")

        // subsequent non-null and non-ignored are fine
        onLog(android.util.Log.ERROR, "sync15:places", "DB stuff broke")
        crashReporter.assertLastException(2, "sync15:places - DB stuff broke")

        // ignored are still ignored
        expectedIgnoredTags.forEach { tag ->
            onLog(android.util.Log.ERROR, tag, "Stuff broke")
            assertEquals(2, crashReporter.exceptions.size)
        }
    }

    @Test
    fun `log priority to level filter`() {
        assertEquals(LogLevelFilter.DEBUG, Log.Priority.DEBUG.asLevelFilter(false))
        assertEquals(LogLevelFilter.TRACE, Log.Priority.DEBUG.asLevelFilter(true))

        assertEquals(LogLevelFilter.INFO, Log.Priority.INFO.asLevelFilter(false))
        assertEquals(LogLevelFilter.INFO, Log.Priority.INFO.asLevelFilter(true))

        assertEquals(LogLevelFilter.WARN, Log.Priority.WARN.asLevelFilter(false))
        assertEquals(LogLevelFilter.WARN, Log.Priority.WARN.asLevelFilter(true))

        assertEquals(LogLevelFilter.ERROR, Log.Priority.ERROR.asLevelFilter(false))
        assertEquals(LogLevelFilter.ERROR, Log.Priority.ERROR.asLevelFilter(true))
    }

    private class TestCrashReporter :
        CrashReporting {
        val exceptions: MutableList<Throwable> = mutableListOf()

        override fun submitCaughtException(throwable: Throwable): Job {
            exceptions.add(throwable)
            return mock()
        }

        override fun recordCrashBreadcrumb(breadcrumb: Breadcrumb) {
            fail()
        }

        fun assertLastException(expectedCount: Int, msg: String) {
            assertEquals(expectedCount, exceptions.size)
            assertTrue(exceptions.last() is RustErrorException)
            assertEquals(msg, exceptions.last().message)
        }
    }
}
