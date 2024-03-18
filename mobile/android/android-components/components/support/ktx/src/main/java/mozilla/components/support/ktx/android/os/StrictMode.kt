/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.android.os

import android.os.StrictMode

/**
 * Runs the given [functionBlock] and sets the ThreadPolicy after its completion.
 *
 * This function is written in the style of [AutoCloseable.use].
 *
 * @return the value returned by [functionBlock].
 */
inline fun <R> StrictMode.ThreadPolicy.resetAfter(functionBlock: () -> R): R = try {
    functionBlock()
} finally {
    StrictMode.setThreadPolicy(this)
}
