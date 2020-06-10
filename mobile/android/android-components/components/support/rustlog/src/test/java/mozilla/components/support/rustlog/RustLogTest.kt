/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.rustlog

import kotlinx.coroutines.Job
import mozilla.components.support.base.crash.Breadcrumb
import mozilla.components.support.base.crash.CrashReporting
import mozilla.components.support.base.log.Log
import org.junit.Test
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.mockito.Mockito.verifyZeroInteractions

class RustLogTest {
    @Test
    fun `maybeSendLogToCrashReporter log processing ignores low priority stuff`() {
        val crashReporter = mock<CrashReporting>()

        RustLog.maybeSendLogToCrashReporter(crashReporter, Log.Priority.DEBUG, "sync15:multiple", "Stuff broke")
        verifyZeroInteractions(crashReporter)

        RustLog.maybeSendLogToCrashReporter(crashReporter, Log.Priority.INFO, "sync15:multiple", "Stuff broke")
        verifyZeroInteractions(crashReporter)

        RustLog.maybeSendLogToCrashReporter(crashReporter, Log.Priority.WARN, "sync15:multiple", "Stuff broke")
        verifyZeroInteractions(crashReporter)

        RustLog.maybeSendLogToCrashReporter(crashReporter, Log.Priority.WARN, null, "Stuff broke")
        verifyZeroInteractions(crashReporter)
    }

    @Test
    fun `maybeSendLogToCrashReporter log processing reports error level stuff`() {
        val crashReporter = TestCrashReporter()
        RustLog.maybeSendLogToCrashReporter(crashReporter, Log.Priority.ERROR, "sync15:multiple", "Stuff broke")
        crashReporter.assertLastException(1, "sync15:multiple - Stuff broke")

        RustLog.maybeSendLogToCrashReporter(crashReporter, Log.Priority.ERROR, null, "Something maybe broke")
        crashReporter.assertLastException(2, "null - Something maybe broke")
    }

    @Test
    fun `maybeSendLogToCrashReporter log processing ignores certain tags`() {
        val expectedIgnoredTags = listOf("viaduct::backend::ffi")
        val crashReporter = TestCrashReporter()

        expectedIgnoredTags.forEach { tag ->
            RustLog.maybeSendLogToCrashReporter(crashReporter, Log.Priority.ERROR, tag, "Stuff broke")
            assertTrue(crashReporter.exceptions.isEmpty())
        }

        // null tags are fine
        RustLog.maybeSendLogToCrashReporter(crashReporter, Log.Priority.ERROR, null, "Stuff broke")
        crashReporter.assertLastException(1, "null - Stuff broke")

        // subsequent non-null and non-ignored are fine
        RustLog.maybeSendLogToCrashReporter(crashReporter, Log.Priority.ERROR, "sync15:places", "DB stuff broke")
        crashReporter.assertLastException(2, "sync15:places - DB stuff broke")

        // ignored are still ignored
        expectedIgnoredTags.forEach { tag ->
            RustLog.maybeSendLogToCrashReporter(crashReporter, Log.Priority.ERROR, tag, "Stuff broke")
            assertEquals(2, crashReporter.exceptions.size)
        }
    }

    private class TestCrashReporter : CrashReporting {
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