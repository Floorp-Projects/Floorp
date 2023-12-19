/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.log.logger

import android.os.SystemClock
import androidx.test.ext.junit.runners.AndroidJUnit4
import mozilla.components.support.base.log.Log
import mozilla.components.support.base.log.fake.FakeLogSink
import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
class LoggerTest {

    private lateinit var sink: FakeLogSink

    @Before
    fun setUp() {
        sink = FakeLogSink()
        Log.addSink(sink)
    }

    @After
    fun tearDown() = Log.reset()

    @Test
    fun `debug calls are forwarded to Log and sinks`() {
        val logger = Logger("Tag")

        val exception = RuntimeException()
        logger.debug(message = "Hello", throwable = exception)

        assertEquals(
            FakeLogSink.Entry(
                priority = Log.Priority.DEBUG,
                tag = "Tag",
                throwable = exception,
                message = "Hello",
            ),
            sink.logs.first(),
        )
    }

    @Test
    fun `info calls are forwarded to Log and sinks`() {
        val logger = Logger("Tag")

        val exception = RuntimeException()
        logger.info(message = "Hello", throwable = exception)

        assertEquals(
            FakeLogSink.Entry(
                priority = Log.Priority.INFO,
                tag = "Tag",
                throwable = exception,
                message = "Hello",
            ),
            sink.logs.first(),
        )
    }

    @Test
    fun `warn calls are forwarded to Log and sinks`() {
        val logger = Logger("Tag")

        val exception = RuntimeException()
        logger.warn(message = "Hello", throwable = exception)

        assertEquals(
            FakeLogSink.Entry(
                priority = Log.Priority.WARN,
                tag = "Tag",
                throwable = exception,
                message = "Hello",
            ),
            sink.logs.first(),
        )
    }

    @Test
    fun `error calls are forwarded to Log and sinks`() {
        val logger = Logger("Tag")

        val exception = RuntimeException()
        logger.error(message = "Hello", throwable = exception)

        assertEquals(
            FakeLogSink.Entry(
                priority = Log.Priority.ERROR,
                tag = "Tag",
                throwable = exception,
                message = "Hello",
            ),
            sink.logs.first(),
        )
    }

    @Test
    fun `Companion object provides methods using shared Logger instance without tag`() {
        val debugException = RuntimeException()
        Logger.debug("debug message", debugException)

        val infoException = RuntimeException()
        Logger.info("info message", infoException)

        val warnException = RuntimeException()
        Logger.warn("warn message", warnException)

        val errorException = RuntimeException()
        Logger.error("error message", errorException)

        val debugLog = FakeLogSink.Entry(
            priority = Log.Priority.DEBUG,
            tag = null,
            throwable = debugException,
            message = "debug message",
        )
        val infoLog = debugLog.copy(
            message = "info message",
            throwable = infoException,
            priority = Log.Priority.INFO,
        )
        val warnLog = debugLog.copy(
            message = "warn message",
            throwable = warnException,
            priority = Log.Priority.WARN,
        )
        val errorLog = debugLog.copy(
            message = "error message",
            throwable = errorException,
            priority = Log.Priority.ERROR,
        )
        val expectedLogs = listOf(debugLog, infoLog, warnLog, errorLog)

        assertEquals(expectedLogs, sink.logs)
    }

    @Test
    fun `measure call logs two messages`() {
        Logger.measure("testing") { /* do nothing */ }

        val firstLog = FakeLogSink.Entry(
            priority = Log.Priority.DEBUG,
            tag = null,
            throwable = null,
            message = "⇢ testing",
        )
        val secondLog = firstLog.copy(message = "⇠ testing [0ms]")
        val expectedLogs = listOf(firstLog, secondLog)

        assertEquals(expectedLogs, sink.logs)
    }

    @Test
    fun `measure call measures time inside block`() {
        val logger = Logger("WithTag")

        logger.measure("testing") {
            // Pretend to do something
            SystemClock.sleep(10)
        }

        val firstLog = FakeLogSink.Entry(
            priority = Log.Priority.DEBUG,
            tag = "WithTag",
            throwable = null,
            message = "⇢ testing",
        )
        val secondLog = firstLog.copy(message = "⇠ testing [10ms]")
        val expectedLogs = listOf(firstLog, secondLog)

        assertEquals(expectedLogs, sink.logs)
    }
}
