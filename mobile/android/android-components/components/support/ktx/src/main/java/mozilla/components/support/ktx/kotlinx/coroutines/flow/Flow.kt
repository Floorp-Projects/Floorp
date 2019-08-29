/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.support.ktx.kotlinx.coroutines.flow

import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.filter

/**
 * Returns a [Flow] containing only values of the original [Flow] that have changed compared to
 * the value emitted before them.
 *
 * Example:
 * Original Flow: A, B, B, C, A, A, A, D, A
 * Returned Flow: A, B, C, A, D, A
 */
fun <T> Flow<T>.ifChanged(): Flow<T> {
    var observedValueOnce = false
    var lastValue: T? = null

    return filter { value ->
        if (!observedValueOnce || value !== lastValue) {
            lastValue = value
            observedValueOnce = true
            true
        } else {
            false
        }
    }
}
