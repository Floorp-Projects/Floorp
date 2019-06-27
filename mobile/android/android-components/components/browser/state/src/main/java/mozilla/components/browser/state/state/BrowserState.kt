/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package mozilla.components.browser.state.state

import mozilla.components.lib.state.State

/**
 * Value type that represents the complete state of the browser/engine.
 *
 * @property tabs the list of open tabs, defaults to an empty list.
 * @property selectedTabId the ID of the currently selected (active) tab.
 * @property customTabs the list of custom tabs, defaults to an empty list.
 */
data class BrowserState(
    val tabs: List<TabSessionState> = emptyList(),
    val selectedTabId: String? = null,

    val customTabs: List<CustomTabSessionState> = emptyList()
) : State
