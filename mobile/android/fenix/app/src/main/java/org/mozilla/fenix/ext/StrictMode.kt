/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.ext

import android.os.StrictMode
import mozilla.components.support.ktx.android.os.resetAfter
import org.mozilla.fenix.Config

/**
 * Runs the given [functionBlock] and sets the ThreadPolicy after its completion in Debug mode.
 * Otherwise simply runs the [functionBlock]
 * This function is written in the style of [AutoCloseable.use].
 * @return the value returned by [functionBlock].
 */
inline fun <R> StrictMode.ThreadPolicy.resetPoliciesAfter(functionBlock: () -> R): R {
    // Calling resetAfter takes 1-2ms (unknown device) so we only execute it if StrictMode can
    // actually be enabled. https://github.com/mozilla-mobile/fenix/issues/11617
    //
    // The expression in this if is duplicated in StrictModeManager.enableStrictMode: see that method
    // for details.
    return if (Config.channel.isDebug) {
        resetAfter { functionBlock() }
    } else {
        functionBlock()
    }
}
