/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.lib.state.helpers

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import mozilla.components.lib.state.Observer
import mozilla.components.lib.state.State

/**
 * Creates an [Observer] that will map the received [State] to [T] (using [map]) and will invoke the callback
 * [then] only if the value has changed from the last mapped value.
 *
 * @param onMainThread Whether or not the [then] function should be executed on the main thread.
 * @param map A function that maps [State] to the value we want to observe for changes.
 * @param then Function that gets invoked whenever the mapped value changed.
 */
@Suppress("FunctionNaming")
fun <S : State, T> onlyIfChanged(
    onMainThread: Boolean = false,
    map: (S) -> T?,
    then: (S, T) -> Unit,
    scope: CoroutineScope = GlobalScope
): Observer<S> {
    var lastValue: T? = null

    return fun (value: S) {
        val mapped = map(value)

        if (mapped !== null && mapped !== lastValue) {
            if (onMainThread) {
                scope.launch(Dispatchers.Main) {
                    then(value, mapped)
                }
            } else {
                then(value, mapped)
            }
            lastValue = mapped
        }
    }
}
