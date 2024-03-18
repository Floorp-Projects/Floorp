/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.ext

import mozilla.components.support.base.log.logger.Logger
import org.json.JSONArray
import org.json.JSONException
import org.json.JSONObject
import java.io.PrintWriter
import java.io.StringWriter
import kotlin.collections.HashSet

private const val STACK_TRACE_INITIAL_BUFFER_SIZE = 256
private const val STACK_TRACE_MAX_LENGTH = 100000
private const val STACK_TRACE_MAX_FRAME = 50

/**
 * Returns a formatted string of the [Throwable.stackTrace].
 */
fun Throwable.getStacktraceAsString(stackTraceMaxLength: Int = STACK_TRACE_MAX_LENGTH): String {
    val sw = StringWriter(STACK_TRACE_INITIAL_BUFFER_SIZE)
    val pw = PrintWriter(sw, false)
    printStackTrace(pw)
    pw.flush()
    return sw.toString().take(stackTraceMaxLength)
}

/**
 * Returns a formatted JSON string of the [Throwable.stackTrace].
 */
@Suppress("NestedBlockDepth")
fun Throwable.getStacktraceAsJsonString(maxFrame: Int = STACK_TRACE_MAX_FRAME): String {
    val throwableList = extractThrowableList()
    val result = JSONObject()
    var frameCount = 0
    try {
        val exception = JSONObject()
        val values = JSONArray()
        for (throwable in throwableList) {
            if (frameCount >= maxFrame) {
                break
            }

            val frames = JSONArray()
            for (stackTraceElement in throwable.stackTrace) {
                if (frameCount >= maxFrame) {
                    break
                }

                val frame = JSONObject()
                frame.put("module", stackTraceElement.className)
                frame.put("function", stackTraceElement.methodName)
                frame.put("in_app", !stackTraceElement.isNativeMethod)
                frame.put("lineno", stackTraceElement.lineNumber)
                frame.put("filename", stackTraceElement.fileName)
                frames.put(frame)
                frameCount++
            }
            val framesObject = JSONObject().put("frames", frames)
            framesObject.put("type", throwable.toString().substringBefore(':').substringAfterLast('.'))
            framesObject.put("module", throwable.toString().substringBefore(':').substringBeforeLast('.'))
            framesObject.put("value", throwable.message)
            values.put(JSONObject().put("stacktrace", framesObject))
        }

        exception.put("values", values)
        result.put("exception", exception)
    } catch (e: JSONException) {
        Logger.warn("Could not parse throwable", e)
    }

    return result.toString()
}

private fun Throwable.extractThrowableList(): List<Throwable> {
    val throwables = ArrayList<Throwable>()
    val circularityDetector = HashSet<Throwable>()
    var currentThrowable: Throwable? = this

    while (currentThrowable != null && circularityDetector.add(currentThrowable)) {
        throwables.add(currentThrowable)
        currentThrowable = currentThrowable.cause
    }

    return throwables.asReversed()
}
