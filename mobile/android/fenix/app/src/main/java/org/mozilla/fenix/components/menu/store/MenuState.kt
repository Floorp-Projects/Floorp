/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.menu.store

import mozilla.components.lib.state.State

/**
 * Value type that represents the state of the menu.
 *
 * @property isBookmarked Whether or not the current selected tab is bookmarked.
 */
data class MenuState(
    val isBookmarked: Boolean = false,
) : State
