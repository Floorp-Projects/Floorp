/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu.store

import mozilla.components.browser.state.state.TabSessionState
import mozilla.components.lib.state.State

/**
 * Value type that represents the state of the menu.
 *
 * @property browserMenuState The [BrowserMenuState] of the current browser session if any.
 */
data class MenuState(
    val browserMenuState: BrowserMenuState? = null,
) : State

/**
 * Value type that represents the state of the browser menu.
 *
 * @property selectedTab The current selected [TabSessionState].
 * @property isBookmarked Whether or not the selected tab is bookmarked.
 */
data class BrowserMenuState(
    val selectedTab: TabSessionState,
    val isBookmarked: Boolean = false,
)
