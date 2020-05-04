/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.ext

import java.io.PrintWriter
import java.io.StringWriter

private const val STACK_TRACE_INITIAL_BUFFER_SIZE = 256

/**
 * Returns a formatted string of the [Throwable.stackTrace].
 */
fun Throwable.getStacktraceAsString(): String {
    val sw = StringWriter(STACK_TRACE_INITIAL_BUFFER_SIZE)
    val pw = PrintWriter(sw, false)
    printStackTrace(pw)
    pw.flush()
    return sw.toString()
}
