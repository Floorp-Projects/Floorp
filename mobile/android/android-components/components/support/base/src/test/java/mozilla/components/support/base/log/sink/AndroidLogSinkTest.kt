/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.log.sink

import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.base.log.Log
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.annotation.Config
import org.robolectric.shadows.ShadowLog
import java.io.PrintWriter

@RunWith(AndroidJUnit4::class)
class AndroidLogSinkTest {

    @Before
    fun setUp() {
        ShadowLog.clear()
    }

    @Test
    fun `debug log will be print to Android log`() {
        val sink = AndroidLogSink()
        sink.log(Log.Priority.DEBUG, "Tag", message = "Hello World!")

        val logs = ShadowLog.getLogs()
        assertEquals(1, logs.size)
        assertEquals("Hello World!", logs.last().msg)
        assertEquals("Tag", logs.last().tag)
        assertNull(logs.last().throwable)
        assertEquals(android.util.Log.DEBUG, logs.last().type)
    }

    @Test
    fun `info log will be print to Android log`() {
        val sink = AndroidLogSink()
        sink.log(Log.Priority.INFO, "Tag", message = "Hello World!")

        val logs = ShadowLog.getLogs()
        assertEquals(1, logs.size)
        assertEquals("Hello World!", logs.last().msg)
        assertEquals("Tag", logs.last().tag)
        assertNull(logs.last().throwable)
        assertEquals(android.util.Log.INFO, logs.last().type)
    }

    @Test
    fun `warn log will be print to Android log`() {
        val sink = AndroidLogSink()
        sink.log(Log.Priority.WARN, "Tag", message = "Hello World!")

        val logs = ShadowLog.getLogs()
        assertEquals(1, logs.size)
        assertEquals("Hello World!", logs.last().msg)
        assertEquals("Tag", logs.last().tag)
        assertNull(logs.last().throwable)
        assertEquals(android.util.Log.WARN, logs.last().type)
    }

    @Test
    fun `error log will be print to Android log`() {
        val sink = AndroidLogSink()
        sink.log(Log.Priority.ERROR, "Tag", message = "Hello World!")

        val logs = ShadowLog.getLogs()
        assertEquals(1, logs.size)
        assertEquals("Hello World!", logs.last().msg)
        assertEquals("Tag", logs.last().tag)
        assertNull(logs.last().throwable)
        assertEquals(android.util.Log.ERROR, logs.last().type)
    }

    @Test
    fun `Sink will use default tag if non is provided`() {
        val sink = AndroidLogSink()
        sink.log(message = "Hello!")

        val logs = ShadowLog.getLogs()
        assertEquals(1, logs.size)
        assertEquals("Hello!", logs.last().msg)
        assertEquals("App", logs.last().tag)
    }

    @Test
    fun `Sink will use provided tag`() {
        val sink = AndroidLogSink("Testing")
        sink.log(message = "What is this?")

        val logs = ShadowLog.getLogs()
        assertEquals(1, logs.size)
        assertEquals("What is this?", logs.last().msg)
        assertEquals("Testing", logs.last().tag)
    }

    @Test
    @Config(sdk = [21])
    fun `Tag will be truncated on SDK 21+`() {
        val sink = AndroidLogSink("ABCDEFGHIJKLMNOPQRSTUVWXYZ123456789")
        sink.log(message = "Hello!")

        val logs = ShadowLog.getLogs()
        assertEquals(1, logs.size)
        assertEquals("Hello!", logs.last().msg)
        assertEquals("ABCDEFGHIJKLMNOPQRSTUVW", logs.last().tag)

        ShadowLog.clear()

        sink.log(message = "Yes!", tag = "1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ")

        val logs2 = ShadowLog.getLogs()

        assertEquals(1, logs2.size)
        assertEquals("Yes!", logs2.last().msg)
        assertEquals("1234567890ABCDEFGHIJKLM", logs2.last().tag)

        ShadowLog.clear()

        sink.log(message = "No!", tag = "Short")

        val logs3 = ShadowLog.getLogs()

        assertEquals(1, logs3.size)
        assertEquals("No!", logs3.last().msg)
        assertEquals("Short", logs3.last().tag)
    }

    @Test
    @Config(sdk = [24])
    fun `Tag will not be truncated on SDK 24+`() {
        val sink = AndroidLogSink("ABCDEFGHIJKLMNOPQRSTUVWXYZ123456789")
        sink.log(message = "Hello!")

        val logs = ShadowLog.getLogs()
        assertEquals(1, logs.size)
        assertEquals("Hello!", logs.last().msg)
        assertEquals("ABCDEFGHIJKLMNOPQRSTUVWXYZ123456789", logs.last().tag)

        ShadowLog.clear()

        sink.log(message = "Yes!", tag = "1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ")

        val logs2 = ShadowLog.getLogs()

        assertEquals(1, logs2.size)
        assertEquals("Yes!", logs2.last().msg)
        assertEquals("1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ", logs2.last().tag)

        ShadowLog.clear()

        sink.log(message = "No!", tag = "Short")

        val logs3 = ShadowLog.getLogs()

        assertEquals(1, logs3.size)
        assertEquals("No!", logs3.last().msg)
        assertEquals("Short", logs3.last().tag)
    }

    @Test
    fun `Sink will print stacktrace of throwable and message`() {
        val sink = AndroidLogSink()

        sink.log(message = "An error occurred", throwable = MockThrowable())

        val logs = ShadowLog.getLogs()
        assertEquals(1, logs.size)
        assertEquals(
            "An error occurred\njava.lang.RuntimeException: This is broken\n\tat A\n\tat B\n\tat C",
            logs.last().msg,
        )
    }

    @Test
    fun `Sink will print stacktrace of throwable`() {
        val sink = AndroidLogSink()

        sink.log(throwable = MockThrowable())

        val logs = ShadowLog.getLogs()
        assertEquals(1, logs.size)
        assertEquals(
            "java.lang.RuntimeException: This is broken\n\tat A\n\tat B\n\tat C",
            logs.last().msg,
        )
    }

    @Test
    fun `Sink will print empty default message if no message and no throwable is provided`() {
        val sink = AndroidLogSink()

        sink.log()
        sink.log(tag = "Tag")

        val logs = ShadowLog.getLogs()

        assertEquals(2, logs.size)

        assertEquals("(empty)", logs[0].msg)
        assertEquals("App", logs[0].tag)

        assertEquals("(empty)", logs[1].msg)
        assertEquals("Tag", logs[1].tag)
    }

    private class MockThrowable : Throwable("Kaput") {
        override fun printStackTrace(writer: PrintWriter) {
            writer.write("java.lang.RuntimeException: This is broken\n\tat A\n\tat B\n\tat C")
        }
    }
}
