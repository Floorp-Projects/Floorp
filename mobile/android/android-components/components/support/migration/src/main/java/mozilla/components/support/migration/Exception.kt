/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.migration

/**
 * Returns a unique identifier for this [Throwable]
 */
fun Exception.uniqueId(): String {
    // In extreme cases, VM is allowed to return an empty stacktrace.
    val topStackFrameString = this.stackTrace.getOrNull(0)?.toString() ?: ""
    return "${this::class.java.canonicalName} $topStackFrameString"
}
