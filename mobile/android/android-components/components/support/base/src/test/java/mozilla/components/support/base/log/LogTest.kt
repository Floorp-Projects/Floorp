/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.log

import mozilla.components.support.base.log.fake.FakeLogSink
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class LogTest {
    @After
    fun tearDown() = Log.reset()

    @Test
    fun `log call will be forwarded to sinks`() {
        val sink = FakeLogSink()
        Log.addSink(sink)

        val exception = RuntimeException()

        Log.log(Log.Priority.DEBUG, "Tag", exception, "Hello World!")

        assertEquals(
            FakeLogSink.Entry(
                priority = Log.Priority.DEBUG,
                tag = "Tag",
                throwable = exception,
                message = "Hello World!",
            ),
            sink.logs.first(),
        )
    }

    @Test
    fun `log call will not be forwarded if log level is too low`() {
        val sink = FakeLogSink()
        Log.addSink(sink)

        val exception = RuntimeException()

        Log.logLevel = Log.Priority.INFO
        Log.log(Log.Priority.DEBUG, "Tag", exception, "Hello World!")

        assertTrue(sink.logs.isEmpty())
    }

    @Test
    fun `log messages with the exact log level will be forwarded`() {
        val sink = FakeLogSink()
        Log.addSink(sink)

        val exception = RuntimeException()

        Log.logLevel = Log.Priority.WARN
        Log.log(Log.Priority.WARN, "Tag", exception, "Hello World!")

        assertEquals(
            FakeLogSink.Entry(
                priority = Log.Priority.WARN,
                tag = "Tag",
                throwable = exception,
                message = "Hello World!",
            ),
            sink.logs.first(),
        )
    }

    @Test
    fun `log messages with higher log level will be forwarded`() {
        val sink = FakeLogSink()
        Log.addSink(sink)

        val exception = RuntimeException()

        Log.logLevel = Log.Priority.WARN
        Log.log(Log.Priority.ERROR, "Tag", exception, "Hello World!")

        assertEquals(
            FakeLogSink.Entry(
                priority = Log.Priority.ERROR,
                tag = "Tag",
                throwable = exception,
                message = "Hello World!",
            ),
            sink.logs.first(),
        )
    }
}
