/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.appstate.home

import org.mozilla.fenix.browser.browsingmode.BrowsingMode
import org.mozilla.fenix.components.appstate.AppAction.ToolbarAction
import org.mozilla.fenix.components.appstate.AppState

/**
 * Reducer to handle updating [AppState] with the result of a [ToolbarAction].
 */
object ToolbarReducer {

    /**
     * Reducer to handle updating [AppState] with the result of a [ToolbarAction].
     */
    fun reduce(state: AppState, action: ToolbarAction): AppState = when (action) {
        is ToolbarAction.NewTab -> state.copy(mode = BrowsingMode.Normal)
        is ToolbarAction.NewPrivateTab -> state.copy(mode = BrowsingMode.Private)
    }
}
