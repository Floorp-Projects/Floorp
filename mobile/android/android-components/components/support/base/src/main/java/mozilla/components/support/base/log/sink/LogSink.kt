/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.base.log.sink

import mozilla.components.support.base.log.Log

interface LogSink {
    fun log(
        priority: Log.Priority = Log.Priority.DEBUG,
        tag: String? = null,
        throwable: Throwable? = null,
        message: String? = null,
    )
}
