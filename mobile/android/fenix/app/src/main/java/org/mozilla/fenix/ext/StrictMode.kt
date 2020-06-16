/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.fenix.ext

import android.os.StrictMode
import org.mozilla.fenix.Config

/**
 * Runs the given [functionBlock] and sets the ThreadPolicy after its completion in Debug mode.
 * Otherwise simply runs the [functionBlock]
 * This function is written in the style of [AutoCloseable.use].
 * @return the value returned by [functionBlock].
 */
inline fun <R> StrictMode.ThreadPolicy.resetPoliciesAfter(functionBlock: () -> R): R {
    return if (Config.channel.isDebug) {
        try {
            functionBlock()
        } finally {
            StrictMode.setThreadPolicy(this)
        }
    } else {
        functionBlock()
    }
}
