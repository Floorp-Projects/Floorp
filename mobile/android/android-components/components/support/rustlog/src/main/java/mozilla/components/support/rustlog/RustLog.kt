/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.rustlog

import mozilla.appservices.rustlog.LogLevelFilter
import mozilla.appservices.rustlog.RustLogAdapter
import mozilla.components.support.base.log.Log

object RustLog {

    /**
     * Enable the Rust log adapter.
     *
     * This does almost (see below) nothing if you are not in a megazord build.
     * After calling this, logs emitted by Rust code are forwarded to any
     * LogSinks attached to [Log].
     *
     * Megazording is required due to each dynamically loaded Rust library having
     * its own internal/private version of the Rust logging framework. When
     * megazording, this is still true, but there's only a single dynamically
     * loaded library, and so it is redirected properly.
     *
     * Note that non-megazord versions of the Rust libraries will log directly to
     * logcat by default (at DEBUG level), so while they cannot hook into the base
     * component log system, they still have logs available for development use.
     *
     * (We say "almost" nothing, as calling this will hook up logging for the dynamic
     * library containing the Rust log hooking (and only that), as well as logging
     * a single message indicating that it completed initialization).
     */
    fun enable() {
        RustLogAdapter.enable { level, tagStr, msgStr ->
            Log.log(levelToPriority(level), tagStr, null, msgStr)
            // Return true to keep open. Eventually we could intercept calls
            // to disable that happen as the direct result of the above call
            // (e.g. on this thread, before this function returns) and return
            // false if any happen, but for now this is fine. (Exceptions thrown
            // by a log sink will also close us)
            true
        }
    }

    /**
     * Disable the rust log adapter.
     */
    fun disable() {
        RustLogAdapter.disable()
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
    fun setMaxLevel(level: Log.Priority, includePII: Boolean = false) {
        val levelFilter = when (level) {
            Log.Priority.DEBUG -> {
                if (includePII) {
                    LogLevelFilter.TRACE
                } else {
                    LogLevelFilter.DEBUG
                }
            }
            Log.Priority.INFO -> LogLevelFilter.INFO
            Log.Priority.WARN -> LogLevelFilter.WARN
            Log.Priority.ERROR -> LogLevelFilter.ERROR
        }
        RustLogAdapter.setMaxLevel(levelFilter)
    }
}

internal fun levelToPriority(level: Int): Log.Priority {
    return when {
        level <= android.util.Log.DEBUG -> Log.Priority.DEBUG
        level == android.util.Log.INFO -> Log.Priority.INFO
        level == android.util.Log.WARN -> Log.Priority.WARN
        else -> Log.Priority.ERROR
    }
}
