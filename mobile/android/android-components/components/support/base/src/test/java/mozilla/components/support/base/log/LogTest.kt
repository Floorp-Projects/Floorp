/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.log

import mozilla.components.support.base.log.sink.LogSink
import mozilla.components.support.test.mock
import org.junit.After
import org.junit.Test
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions

class LogTest {
    @After
    fun tearDown() = Log.reset()

    @Test
    fun `log call will be forwarded to sinks`() {
        val sink: LogSink = mock()
        Log.addSink(sink)

        val exception = RuntimeException()

        Log.log(Log.Priority.DEBUG, "Tag", exception, "Hello World!")

        verify(sink).log(
            priority = Log.Priority.DEBUG,
            tag = "Tag",
            throwable = exception,
            message = "Hello World!",
        )
    }

    @Test
    fun `log call will not be forwarded if log level is too low`() {
        val sink: LogSink = mock()
        Log.addSink(sink)

        val exception = RuntimeException()

        Log.logLevel = Log.Priority.INFO
        Log.log(Log.Priority.DEBUG, "Tag", exception, "Hello World!")

        verifyNoMoreInteractions(sink)
    }

    @Test
    fun `log messages with the exact log level will be forwarded`() {
        val sink: LogSink = mock()
        Log.addSink(sink)

        val exception = RuntimeException()

        Log.logLevel = Log.Priority.WARN
        Log.log(Log.Priority.WARN, "Tag", exception, "Hello World!")

        verify(sink).log(
            priority = Log.Priority.WARN,
            tag = "Tag",
            throwable = exception,
            message = "Hello World!",
        )
    }

    @Test
    fun `log messages with higher log level will be forwarded`() {
        val sink: LogSink = mock()
        Log.addSink(sink)

        val exception = RuntimeException()

        Log.logLevel = Log.Priority.WARN
        Log.log(Log.Priority.ERROR, "Tag", exception, "Hello World!")

        verify(sink).log(
            priority = Log.Priority.ERROR,
            tag = "Tag",
            throwable = exception,
            message = "Hello World!",
        )
    }
}
