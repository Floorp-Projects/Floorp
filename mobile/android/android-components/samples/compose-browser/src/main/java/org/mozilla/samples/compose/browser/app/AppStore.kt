/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.compose.browser.app

import mozilla.components.lib.state.Store

/**
 * [Store] for the global [AppState].
 */
class AppStore : Store<AppState, AppAction>(
    initialState = AppState(),
    reducer = ::reduce,
)

private fun reduce(appState: AppState, appAction: AppAction): AppState {
    if (appAction is AppAction.ToggleTheme) {
        return appState.copy(theme = (appState.theme + 1) % 2)
    }
    return appState
}
