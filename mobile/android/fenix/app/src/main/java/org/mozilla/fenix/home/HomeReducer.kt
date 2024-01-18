/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.home

import org.mozilla.fenix.components.appstate.AppAction.HomeAction
import org.mozilla.fenix.components.appstate.AppState

/**
 * Reducer to handle updating [AppState] with the result of an [HomeAction].
 */
object HomeReducer {

    /**
     * Reducer to handle updating [AppState] with the result of an [HomeAction].
     */
    fun reduce(state: AppState, action: HomeAction): AppState = when (action) {
        is HomeAction.OpenToHome -> state.copy(mode = action.mode)
    }
}
