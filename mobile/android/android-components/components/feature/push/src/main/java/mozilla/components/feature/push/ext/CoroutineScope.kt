/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.feature.push.ext

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import mozilla.appservices.push.PushException

/**
 * Catches all fatal push errors to notify the callback before re-throwing.
 */
internal fun CoroutineScope.launchAndTry(
    errorBlock: (Exception) -> Unit = {},
    block: suspend CoroutineScope.() -> Unit,
): Job {
    return launch {
        try {
            block()
        } catch (e: PushException) {
            errorBlock(e)

            // rethrow
            throw e
        }
    }
}
