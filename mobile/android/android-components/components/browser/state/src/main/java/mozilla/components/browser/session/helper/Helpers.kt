/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.helper

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.GlobalScope
import kotlinx.coroutines.launch
import mozilla.components.browser.session.state.BrowserState
import mozilla.components.browser.session.store.Observer

/**
 * Creates an [Observer] that will map the received [BrowserState] to [T] (using [map]) and will invoke the callback
 * [then] only if the value has changed from the last mapped value.
 *
 * @param onMainThread Whether or not the [then] function should be executed on the main thread.
 * @param map A function that maps [BrowserState] to the value we want to observe for changes.
 * @param then Function that gets invoked whenever the mapped value changed.
 */
@Suppress("FunctionNaming")
fun <T> onlyIfChanged(
    onMainThread: Boolean = false,
    map: (BrowserState) -> T?,
    then: (BrowserState, T) -> Unit
): Observer {
    var lastValue: T? = null

    return fun (value: BrowserState) {
        val mapped = map(value)

        if (mapped !== null && mapped !== lastValue) {
            if (onMainThread) {
                GlobalScope.launch(Dispatchers.Main) {
                    then(value, mapped)
                }
            } else {
                then(value, mapped)
            }
            lastValue = mapped
        }
    }
}
