/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.view

import android.view.MotionEvent

/**
 * Executes the given [functionBlock] function on this resource and then closes it down correctly whether
 * an exception is thrown or not. This is inspired by [java.lang.AutoCloseable.use].
 */
inline fun <R> MotionEvent.use(functionBlock: (MotionEvent) -> R): R {
    try {
        return functionBlock(this)
    } finally {
        recycle()
    }
}
