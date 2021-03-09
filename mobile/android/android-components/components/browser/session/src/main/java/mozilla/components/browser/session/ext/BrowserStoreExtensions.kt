/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.session.ext

import kotlinx.coroutines.runBlocking
import mozilla.components.browser.state.action.BrowserAction
import mozilla.components.browser.state.store.BrowserStore
import mozilla.components.browser.state.state.BrowserState
import mozilla.components.lib.state.Store

/**
 * Dispatches [action] on [BrowserStore] and blocks the current thread until the [action] was processed and a new
 * [BrowserState] was created.
 *
 * This is helpful for the migration phase where we want to keep `SessionManager` / `Session` and `BrowserState` in
 * sync when a legacy method returns.
 */
internal fun Store<BrowserState, BrowserAction>.syncDispatch(action: BrowserAction) {
    runBlocking {
        dispatch(action).join()
    }
}
