/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.store

import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.InitAction
import mozilla.components.browser.state.reducer.BrowserStateReducer
import mozilla.components.browser.state.selector.findTab
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.lib.state.Action
import mozilla.components.lib.state.Middleware
import mozilla.components.lib.state.Store
import java.lang.IllegalArgumentException

/**
 * The [BrowserStore] holds the [BrowserState] (state tree).
 *
 * The only way to change the [BrowserState] inside [BrowserStore] is to dispatch an [Action] on it.
 */
class BrowserStore(
    initialState: BrowserState = BrowserState(),
    middleware: List<Middleware<BrowserState, BrowserAction>> = emptyList(),
) : Store<BrowserState, BrowserAction>(
    initialState,
    BrowserStateReducer::reduce,
    middleware,
    "BrowserStore",
) {
    init {
        initialState.selectedTabId?.let {
            if (state.findTab(it) == null) {
                throw IllegalArgumentException("Selected tab does not exist")
            }
        }

        if (initialState.tabs
            .groupingBy { it.id }
            .eachCount()
            .filter { it.value > 1 }
            .isNotEmpty()
        ) {
            throw IllegalArgumentException("Duplicate tabs found")
        }

        dispatch(InitAction)
    }
}
