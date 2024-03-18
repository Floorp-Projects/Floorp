/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.log.fake

import mozilla.components.support.base.log.Log
import mozilla.components.support.base.log.sink.LogSink

class FakeLogSink : LogSink {

    val logs = mutableListOf<Entry>()

    data class Entry(
        val priority: Log.Priority,
        val tag: String?,
        val throwable: Throwable?,
        val message: String,
    )

    override fun log(priority: Log.Priority, tag: String?, throwable: Throwable?, message: String) {
        logs.add(Entry(priority, tag, throwable, message))
    }
}
