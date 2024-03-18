/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.log.sink

import mozilla.components.support.base.log.Log

/**
 * [LogSink] implementation that prints to console.
 */
internal class TestModeLogSink : LogSink {

    override fun log(priority: Log.Priority, tag: String?, throwable: Throwable?, message: String) {
        val printMessage = buildString {
            append("${priority.name[0]} ")
            if (tag != null) {
                append("[$tag] ")
            }
            append(message)
        }
        println(printMessage)
        throwable?.printStackTrace()
    }
}
