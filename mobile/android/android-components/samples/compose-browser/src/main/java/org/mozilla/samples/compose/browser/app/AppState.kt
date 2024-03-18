/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.samples.compose.browser.app

import mozilla.components.lib.state.State

/**
 * Global state the browser is in (regardless of the currently displayed screen).
 */
data class AppState(
    val theme: Int = 1,
) : State
