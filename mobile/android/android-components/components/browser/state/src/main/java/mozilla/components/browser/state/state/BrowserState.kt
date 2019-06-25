/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state

import mozilla.components.lib.state.State

/**
 * Value type that represents the complete state of the browser/engine.
 */
data class BrowserState(
    val tabs: List<TabSessionState> = emptyList(),
    val selectedTabId: String? = null,

    val customTabs: List<CustomTabSessionState> = emptyList()
) : State
