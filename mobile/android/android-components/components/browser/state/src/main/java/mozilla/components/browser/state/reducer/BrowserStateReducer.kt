/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.reducer

import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.action.ContentAction
import mozilla.components.browser.state.action.CustomTabListAction
import mozilla.components.browser.state.action.EngineAction
import mozilla.components.browser.state.action.SystemAction
import mozilla.components.browser.state.action.TabListAction
import mozilla.components.browser.state.action.TrackingProtectionAction
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.lib.state.Action

/**
 * Reducers for [BrowserStore].
 *
 * A reducer is a function that receives the current [BrowserState] and an [Action] and then returns a new
 * [BrowserState].
 */
internal object BrowserStateReducer {
    fun reduce(state: BrowserState, action: BrowserAction): BrowserState {
        return when (action) {
            is ContentAction -> ContentStateReducer.reduce(state, action)
            is CustomTabListAction -> CustomTabListReducer.reduce(state, action)
            is SystemAction -> SystemReducer.reduce(state, action)
            is TabListAction -> TabListReducer.reduce(state, action)
            is TrackingProtectionAction -> TrackingProtectionStateReducer.reduce(state, action)
            is EngineAction -> EngineStateReducer.reduce(state, action)
        }
    }
}
