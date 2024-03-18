/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.test.ext

import kotlinx.coroutines.Job
import kotlinx.coroutines.runBlocking

/**
 * Blocks the current thread until the job is complete.
 */
fun Job.joinBlocking() {
    runBlocking { join() }
}
