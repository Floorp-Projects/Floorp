/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.rustlog

import androidx.test.ext.junit.runners.AndroidJUnit4
import kotlinx.coroutines.Job
import mozilla.appservices.rust_log_forwarder.Level
import mozilla.components.concept.base.crash.Breadcrumb
import mozilla.components.concept.base.crash.CrashReporting
import mozilla.components.support.base.log.Log
import mozilla.components.support.test.mock
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Assert.fail
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class RustLogTest {
    @Test
    fun `basic RustLog interactions do not blow up`() {
        RustLog.enable()
        RustLog.setMaxLevel(Log.Priority.DEBUG, false)
        RustLog.setMaxLevel(Log.Priority.DEBUG, true)
        RustLog.setMaxLevel(Log.Priority.INFO, false)
        RustLog.setMaxLevel(Log.Priority.INFO, true)
        RustLog.setMaxLevel(Log.Priority.WARN, false)
        RustLog.setMaxLevel(Log.Priority.WARN, true)
        RustLog.setMaxLevel(Log.Priority.ERROR, false)
        RustLog.setMaxLevel(Log.Priority.ERROR, true)
        RustLog.disable()
        RustLog.enable()
    }

    @Test
    fun `log priority to level filter`() {
        assertEquals(Level.DEBUG, Log.Priority.DEBUG.asLevel(false))
        assertEquals(Level.TRACE, Log.Priority.DEBUG.asLevel(true))

        assertEquals(Level.INFO, Log.Priority.INFO.asLevel(false))
        assertEquals(Level.INFO, Log.Priority.INFO.asLevel(true))

        assertEquals(Level.WARN, Log.Priority.WARN.asLevel(false))
        assertEquals(Level.WARN, Log.Priority.WARN.asLevel(true))

        assertEquals(Level.ERROR, Log.Priority.ERROR.asLevel(false))
        assertEquals(Level.ERROR, Log.Priority.ERROR.asLevel(true))
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
