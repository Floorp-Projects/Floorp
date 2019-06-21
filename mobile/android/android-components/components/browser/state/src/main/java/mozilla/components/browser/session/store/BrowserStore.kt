/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.store

import mozilla.components.browser.session.action.BrowserAction
import mozilla.components.browser.session.reducer.BrowserReducers
import mozilla.components.browser.session.state.BrowserState
import mozilla.components.lib.state.Action
import mozilla.components.lib.state.Store

/**
 * The [BrowserStore] holds the [BrowserState] (state tree).
 *
 * The only way to change the [BrowserState] inside [BrowserStore] is to dispatch an [Action] on it.
 */
class BrowserStore(
    initialState: BrowserState
) : Store<BrowserState, BrowserAction>(
    initialState,
    BrowserReducers::reduce
)
