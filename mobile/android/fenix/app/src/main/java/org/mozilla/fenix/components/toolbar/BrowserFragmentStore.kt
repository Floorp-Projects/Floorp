/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

@file:Suppress("unused", "UNUSED_PARAMETER")

package org.mozilla.fenix.components.toolbar

import mozilla.components.lib.state.Action
import mozilla.components.lib.state.State
import mozilla.components.lib.state.Store

// The state that used to live in this class was moved into another component in #4281. Keeping
// the shell of this file because we will need to expand it as we add additional features to
// the browser.
class BrowserFragmentStore(initialState: BrowserFragmentState) :
    Store<BrowserFragmentState, BrowserFragmentAction>(initialState, ::browserStateReducer)

/**
 * The state for the Browser Screen
 */
class BrowserFragmentState : State

sealed class BrowserFragmentAction : Action

/**
 * Reducers for [BrowserFragmentStore].
 *
 * A top level reducer that receives the current [BrowserFragmentState] and an [Action] and then
 * delegates to the proper child
 */
private fun browserStateReducer(
    state: BrowserFragmentState,
    action: BrowserFragmentAction,
): BrowserFragmentState {
    return when {
        else -> BrowserFragmentState()
    }
}
