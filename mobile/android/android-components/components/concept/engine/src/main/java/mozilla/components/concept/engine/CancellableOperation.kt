/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.concept.engine

import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.Deferred

/**
 * Represents an async operation that can be cancelled.
 */
interface CancellableOperation {

    /**
     * Implementation of [CancellableOperation] that does nothing (for
     * testing purposes or implementing default methods.)
     */
    class Noop : CancellableOperation {
        override fun cancel(): Deferred<Boolean> {
            return CompletableDeferred(true)
        }
    }

    /**
     * Cancels this operation.
     *
     * @return a deferred value indicating whether or not cancellation was successful.
     */
    fun cancel(): Deferred<Boolean>
}
