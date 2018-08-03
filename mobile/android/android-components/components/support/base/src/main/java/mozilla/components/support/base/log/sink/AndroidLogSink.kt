/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.log.sink

import android.os.Build
import mozilla.components.support.base.log.Log
import java.io.PrintWriter
import java.io.StringWriter

private const val MAX_TAG_LENGTH = 23
private const val STACK_TRACE_INITIAL_BUFFER_SIZE = 256

/**
 * <code>LogSink</code> implementation that writes to Android's log.
 *
 * @param defaultTag A default tag that should be used for all logging calls without tag.
 */
class AndroidLogSink(
    private val defaultTag: String = "App"
) : LogSink {
    /**
     * Low-level logging call.
     */
    override fun log(priority: Log.Priority, tag: String?, throwable: Throwable?, message: String?) {
        val logTag = tag(tag)

        val logMessage: String = if (message != null && throwable != null) {
            "$message\n${stackTrace(throwable)}"
        } else message ?: if (throwable != null) {
            stackTrace(throwable)
        } else {
            "(empty)"
        }

        android.util.Log.println(priority.value, logTag, logMessage)
    }

    private fun tag(candidate: String?): String {
        val tag = candidate ?: defaultTag
        if (Build.VERSION.SDK_INT < android.os.Build.VERSION_CODES.N && tag.length > MAX_TAG_LENGTH) {
            return tag.substring(0, MAX_TAG_LENGTH)
        }
        return tag
    }

    private fun stackTrace(throwable: Throwable): String {
        val sw = StringWriter(STACK_TRACE_INITIAL_BUFFER_SIZE)
        val pw = PrintWriter(sw, false)
        throwable.printStackTrace(pw)
        pw.flush()
        return sw.toString()
    }
}
