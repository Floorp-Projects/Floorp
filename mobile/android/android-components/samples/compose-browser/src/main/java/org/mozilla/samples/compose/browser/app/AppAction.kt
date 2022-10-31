/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.compose.browser.app

import mozilla.components.lib.state.Action

/**
 * Actions for updating the global [AppState] via [AppStore].
 */
sealed class AppAction : Action {
    /**
     * Toggles the theme of the app (only for testing purposes).
     */
    object ToggleTheme : AppAction()
}
