/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home.intent

import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.appstate.AppAction.IntentAction
import org.mozilla.fenix.components.appstate.AppState

/**
 * Reducer to handle updating [AppState] with the result of an [IntentAction].
 */
object IntentReducer {

    /**
     * Reducer to handle updating [AppState] with the result of an [IntentAction].
     */
    fun reduce(state: AppState, action: IntentAction): AppState = when (action) {
        is IntentAction.EnterPrivateBrowsing -> state.copy(mode = BrowsingMode.Private)
    }
}
