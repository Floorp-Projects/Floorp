/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.log.logger

import android.os.SystemClock
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.base.log.Log
import mozilla.components.support.base.log.sink.LogSink
import mozilla.components.support.test.mock
import org.junit.After
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.mockito.Mockito.verify
import org.mockito.Mockito.verifyNoMoreInteractions

@RunWith(AndroidJUnit4::class)
class LoggerTest {

    lateinit var sink: LogSink

    @Before
    fun setUp() {
        sink = mock()
        Log.addSink(sink)
    }

    @After
    fun tearDown() = Log.reset()

    @Test
    fun `debug calls are forwarded to Log and sinks`() {
        val logger = Logger("Tag")

        val exception = RuntimeException()
        logger.debug(message = "Hello", throwable = exception)

        verify(sink).log(
            priority = Log.Priority.DEBUG,
            tag = "Tag",
            throwable = exception,
            message = "Hello",
        )
    }

    @Test
    fun `info calls are forwarded to Log and sinks`() {
        val logger = Logger("Tag")

        val exception = RuntimeException()
        logger.info(message = "Hello", throwable = exception)

        verify(sink).log(
            priority = Log.Priority.INFO,
            tag = "Tag",
            throwable = exception,
            message = "Hello",
        )
    }

    @Test
    fun `warn calls are forwarded to Log and sinks`() {
        val logger = Logger("Tag")

        val exception = RuntimeException()
        logger.warn(message = "Hello", throwable = exception)

        verify(sink).log(
            priority = Log.Priority.WARN,
            tag = "Tag",
            throwable = exception,
            message = "Hello",
        )
    }

    @Test
    fun `error calls are forwarded to Log and sinks`() {
        val logger = Logger("Tag")

        val exception = RuntimeException()
        logger.error(message = "Hello", throwable = exception)

        verify(sink).log(
            priority = Log.Priority.ERROR,
            tag = "Tag",
            throwable = exception,
            message = "Hello",
        )
    }

    @Test
    fun `Companion object provides methods using shared Logger instance without tag`() {
        val debugException = RuntimeException()
        Logger.debug("debug message", debugException)

        verify(sink).log(
            priority = Log.Priority.DEBUG,
            tag = null,
            throwable = debugException,
            message = "debug message",
        )

        val infoException = RuntimeException()
        Logger.info("info message", infoException)

        verify(sink).log(
            priority = Log.Priority.INFO,
            tag = null,
            throwable = infoException,
            message = "info message",
        )

        val warnException = RuntimeException()
        Logger.warn("warn message", warnException)

        verify(sink).log(
            priority = Log.Priority.WARN,
            tag = null,
            throwable = warnException,
            message = "warn message",
        )

        val errorException = RuntimeException()
        Logger.error("error message", errorException)

        verify(sink).log(
            priority = Log.Priority.ERROR,
            tag = null,
            throwable = errorException,
            message = "error message",
        )

        verifyNoMoreInteractions(sink)
    }

    @Test
    fun `measure call logs two messages`() {
        Logger.measure("testing") { /* do nothing */ }

        verify(sink).log(
            priority = Log.Priority.DEBUG,
            tag = null,
            throwable = null,
            message = "⇢ testing",
        )

        verify(sink).log(
            priority = Log.Priority.DEBUG,
            tag = null,
            throwable = null,
            message = "⇠ testing [0ms]",
        )

        verifyNoMoreInteractions(sink)
    }

    @Test
    fun `measure call measures time inside block`() {
        val logger = Logger("WithTag")

        logger.measure("testing") {
            // Pretend to do something
            SystemClock.sleep(10)
        }

        verify(sink).log(
            priority = Log.Priority.DEBUG,
            tag = "WithTag",
            throwable = null,
            message = "⇢ testing",
        )

        verify(sink).log(
            priority = Log.Priority.DEBUG,
            tag = "WithTag",
            throwable = null,
            message = "⇠ testing [10ms]",
        )

        verifyNoMoreInteractions(sink)
    }
}
