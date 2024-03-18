/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.log.sink

import mozilla.components.support.base.log.Log

/**
 * Common interface for log sinks.
 */
interface LogSink {

    /**
     * Low-level logging call that should be implemented based on the Sink's capabilities.
     *
     * @param priority The [Log.Priority] of the log message, defaults to [Log.Priority.DEBUG].
     * @param tag The tag of the log message.
     * @param throwable The optional [Throwable] that should be logged.
     * @param message The message that should be logged.
     */
    fun log(
        priority: Log.Priority = Log.Priority.DEBUG,
        tag: String? = null,
        throwable: Throwable? = null,
        message: String,
    )
}
