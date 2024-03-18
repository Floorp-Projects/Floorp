/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.rustlog

import androidx.annotation.VisibleForTesting
import mozilla.appservices.rust_log_forwarder.AppServicesLogger
import mozilla.appservices.rust_log_forwarder.Level
import mozilla.appservices.rust_log_forwarder.Record
import mozilla.appservices.rust_log_forwarder.setLogger
import mozilla.appservices.rust_log_forwarder.setMaxLevel
import mozilla.components.support.base.log.Log

internal class RustErrorException(tag: String?, msg: String) : Exception("$tag - $msg")

object RustLog {

    /**
     * Enable the Rust log adapter.
     *
     * After calling this, logs emitted by Rust code are forwarded to any
     * LogSinks attached to [Log].
     */
    fun enable() {
        setLogger(ForwardOnLog())
    }

    /**
     * Disable the rust log adapter.
     */
    fun disable() {
        setLogger(null)
    }

    /**
     * Set the maximum level of logs that will be forwarded to [Log]. By
     * default, the max level is DEBUG.
     *
     * This is somewhat redundant with [Log.logLevel] (and a stricter
     * filter on Log.logLevel will take precedence here), however
     * setting the max level here can improve performance a great deal,
     * as it allows the Rust code to skip a great deal of work.
     *
     * This includes a `includePII` flag, which allows enabling logs at
     * the trace level. It is ignored if level is not [Log.Priority.DEBUG].
     * These trace level logs* may contain the personal information of users
     * but can be very helpful for tracking down bugs.
     *
     * @param level The maximum (inclusive) level to include logs at.
     * @param includePII If `level` is [Log.Priority.DEBUG], allow
     *     debug logs to contain PII.
     */
    fun setMaxLevel(priority: Log.Priority, includePII: Boolean = false) {
        setMaxLevel(priority.asLevel(includePII))
    }
}

@VisibleForTesting
internal class ForwardOnLog : AppServicesLogger {
    override fun log(record: Record) {
        Log.log(record.level.asLogPriority(), record.target, null, record.message)
    }
}

@VisibleForTesting
internal fun Log.Priority.asLevel(includePII: Boolean): Level {
    return when (this) {
        Log.Priority.DEBUG -> {
            if (includePII) {
                Level.TRACE
            } else {
                Level.DEBUG
            }
        }
        Log.Priority.INFO -> Level.INFO
        Log.Priority.WARN -> Level.WARN
        Log.Priority.ERROR -> Level.ERROR
    }
}

internal fun Level.asLogPriority(): Log.Priority {
    return when (this) {
        // No direct mapping for TRACE, but DEBUG is the closest
        Level.TRACE -> Log.Priority.DEBUG
        Level.DEBUG -> Log.Priority.DEBUG
        Level.INFO -> Log.Priority.INFO
        Level.WARN -> Log.Priority.WARN
        Level.ERROR -> Log.Priority.ERROR
    }
}
