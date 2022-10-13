/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.log

import androidx.annotation.VisibleForTesting
import mozilla.components.support.base.log.sink.LogSink

/**
 * API for logging messages and exceptions.
 *
 * This class does not process any logging calls itself. Instead it forwards the calls to registered
 * <code>LogSink</code> implementations.
 *
 * This class only provides a low-level logging call. The <code>logger</code> sub packages contains
 * logger implementations that wrap <code>Log</code> and provide more convenient APIs.
 */
object Log {
    /**
     * Minimum log level that log calls need to have to be forwarded to registered sinks. Log calls
     * with a lower log level will be ignored.
     */
    var logLevel: Priority = Priority.DEBUG

    private val sinks = mutableListOf<LogSink>()

    private val testMode: Boolean = System.getProperty("logging.test-mode") == "true"

    /**
     * Adds a sink that will receive log calls.
     */
    fun addSink(sink: LogSink) {
        synchronized(sinks) {
            sinks.add(sink)
        }
    }

    /**
     * Low-level logging call.
     *
     * @param priority The priority/type of this log message. By default DEBUG is used.
     * @param tag Used to identify the source of a log message. It usually identifies the class
     *            where the log call occurs.
     * @param throwable An exception to log.
     * @param message A message to be logged.
     */
    fun log(
        priority: Priority = Priority.DEBUG,
        tag: String? = null,
        throwable: Throwable? = null,
        message: String? = null,
    ) {
        if (priority.value >= logLevel.value) {
            synchronized(sinks) {
                sinks.forEach { sink ->
                    sink.log(priority, tag, throwable, message)
                }
            }
        }

        if (testMode) {
            printTestModeMessage(priority, tag, throwable, message)
        }
    }

    // Only for testing
    @VisibleForTesting
    fun reset() {
        logLevel = Priority.DEBUG

        synchronized(sinks) {
            sinks.clear()
        }
    }

    private fun printTestModeMessage(
        priority: Priority,
        tag: String?,
        throwable: Throwable?,
        message: String?,
    ) {
        val printMessage = StringBuilder()
        printMessage.append(priority.name[0])
        printMessage.append(" ")
        if (tag != null) {
            printMessage.append("[$tag] ")
        }
        if (message != null) {
            printMessage.append(message)
        }

        println(printMessage.toString())
        throwable?.printStackTrace()
    }

    /**
     * Priority constants for logging calls.
     */
    enum class Priority(val value: Int) {
        // For simplicity the values mirror the Android log constants values:
        // https://android.googlesource.com/platform/frameworks/base/+/refs/heads/master/core/java/android/util/Log.java
        //
        // We intentionally omit ASSERT and VERBOSE. If you change this,
        // be aware of the impact on consumers.

        DEBUG(android.util.Log.DEBUG),
        INFO(android.util.Log.INFO),
        WARN(android.util.Log.WARN),
        ERROR(android.util.Log.ERROR),
    }
}
