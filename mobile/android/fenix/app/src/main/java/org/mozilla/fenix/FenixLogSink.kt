/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix

import mozilla.components.support.base.log.Log
import mozilla.components.support.base.log.sink.AndroidLogSink
import mozilla.components.support.base.log.sink.LogSink

/**
 * Fenix [LogSink] implementation that writes to Android's log, depending on settings.
 *
 * @param logsDebug If set to false, removes logging of debug logs.
 * @param androidLogSink an [AndroidLogSink] that writes to Android's log.
 */
class FenixLogSink(
    private val logsDebug: Boolean = true,
    private val androidLogSink: LogSink,
) : LogSink {

    override fun log(
        priority: Log.Priority,
        tag: String?,
        throwable: Throwable?,
        message: String?,
    ) {
        if (priority == Log.Priority.DEBUG && !logsDebug) {
            return
        }

        androidLogSink.log(priority, tag, throwable, message)
    }
}
