/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.fenix.components.appstate

import mozilla.components.lib.state.State

/**
 * Value type that represents the state of the tabs tray.
 *
 * @property inactiveTabsExpanded A flag to know if the Inactive Tabs section of the Tabs Tray
 * should be expanded when the tray is opened.
 */
data class AppState(
    val inactiveTabsExpanded: Boolean = false
) : State
